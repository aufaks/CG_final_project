"""Mixamo에서 받은 다른 캐릭터(원본)의 뼈대·애니메이션을 체형에 맞춰 옮겨서 glTF를 만든다 (리타깃).

Mixamo에서 직접 리깅하지 못한 손님(Fred, Morty)용. 원본 캐릭터의 FBX(download/<원본>/*.fbx)를 읽어
뼈대 관절 위치만 대상 체형(아래 TARGETS의 기준점)으로 옮기고, 클립(Idle·Walk·StandToSit·SitIdle)은 그대로 쓴다.
대상 메시(upload/<대상>.zip)에는 Blender 자동 가중치를 계산하고, 못 붙인 정점은 가장 가까운 뼈에 붙인다.

기준점 (단위 cm, 키 170, 발 가운데가 원점, x = 좌우, y = 높이):
    hip       Hips 관절 높이 (사타구니 조금 위)
    leg_x     다리(UpLeg) 좌우 위치
    shoulder  팔이 붙는 높이 (T포즈 팔 높이)
    arm_x     어깨 관절(Arm) 좌우 위치 = 몸통 옆면
    tip_x     T포즈 손끝 좌우 위치
    neck      목(Neck) 높이
    head      머리(Head) 관절 높이

실행 (CGFP 폴더, tools가 있는 곳에서):
    blender --background --factory-startup --python tools/retarget_mixamo.py -- [대상 이름...]
결과: anim/<대상>.gltf (+ .bin, 텍스처)
"""
import os
import shutil
import sys
import tempfile

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mixamo_to_gltf as mx  # noqa: E402

MODEL_HEIGHT = 170.0  # upload/*.zip 모델 키 (tools/prepare_mixamo.py)

TARGETS = {
    # 지느러미 팔은 두께 없는 판(키 0.63 높이), 몸통은 통짜, 다리는 바지
    'Fred': {'source': 'FishG', 'hip': 45.0, 'leg_x': 16.0, 'shoulder': 107.0, 'arm_x': 19.0, 'tip_x': 78.0,
             'neck': 115.0, 'head': 125.0},
    # 머리 = 몸통인 둥근 몸, 가는 막대 팔(키 0.53 높이), 짧은 다리
    'Morty': {'source': 'FishG', 'hip': 40.0, 'leg_x': 10.0, 'shoulder': 90.0, 'arm_x': 30.0, 'tip_x': 110.0,
              'neck': 100.0, 'head': 110.0},
}


def bone_group(name):
    if any(k in name for k in ('Leg', 'Foot', 'Toe')):
        return 'leg'
    if any(k in name for k in ('Shoulder', 'Arm', 'Hand')):
        return 'arm'
    return 'body'


def piecewise(x, xs, ys):
    """xs(오름차순) → ys 구간 선형. 범위 밖은 양 끝 구간을 늘린다."""
    for i in range(len(xs) - 1):
        if x <= xs[i + 1] or i == len(xs) - 2:
            t = (x - xs[i]) / (xs[i + 1] - xs[i])
            return ys[i] + (ys[i + 1] - ys[i]) * t
    return ys[-1]


def measure_source(armature, to_cm):
    """원본 뼈대에서 기준점을 잰다 (cm)."""
    def head(name):
        return to_cm(armature.matrix_world @ armature.data.bones['mixamorig:' + name].head_local)
    tip = max(abs(to_cm(armature.matrix_world @ b.tail_local).x) for b in armature.data.bones)
    return {
        'hip': head('Hips').z, 'leg_x': abs(head('LeftUpLeg').x), 'shoulder': head('LeftArm').z,
        'arm_x': abs(head('LeftArm').x), 'tip_x': tip, 'neck': head('Neck').z, 'head': head('Head').z,
    }


def make_mapper(src, dst):
    def map_point(p, group):  # p: cm (x 좌우, y 앞뒤, z 높이)
        x, y, z = p
        side = 1.0 if x >= 0 else -1.0
        if group == 'leg':
            return Vector((side * (dst['leg_x'] + abs(x) - src['leg_x']), y, z * dst['hip'] / src['hip']))
        if group == 'arm':
            ax = piecewise(abs(x), [0.0, src['arm_x'], src['tip_x']], [0.0, dst['arm_x'], dst['tip_x']])
            return Vector((side * ax, y, dst['shoulder'] + z - src['shoulder']))
        nz = piecewise(z, [0.0, src['hip'], src['neck'], src['head'], MODEL_HEIGHT],
                       [0.0, dst['hip'], dst['neck'], dst['head'], MODEL_HEIGHT])
        return Vector((x, y, nz))
    return map_point


def load_source(source, target):
    """원본 FBX를 전부 읽어 뼈대 하나 + 클립 목록 + 원본 프록시 메시를 돌려준다."""
    folder = os.path.join(mx.DOWNLOAD, source)
    armature, proxy, clips = None, None, []
    for name in sorted(f for f in os.listdir(folder) if f.lower().endswith('.fbx')):
        clip = os.path.splitext(name)[0]
        objects = mx.import_fbx(os.path.join(folder, name))
        arm = next((o for o in objects if o.type == 'ARMATURE'), None)
        action = arm.animation_data.action if arm and arm.animation_data else None
        if action:
            action.name = clip
            action.use_fake_user = True
            clips.append((clip, action))
        if armature is None and arm is not None:
            armature = arm
            armature.name = target
            proxy = next((o for o in objects if o.type == 'MESH'), None)
            continue
        for o in objects:
            bpy.data.objects.remove(o, do_unlink=True)
    return armature, proxy, clips


def nearest_bone_weights(mesh, armature, indices):
    """자동 가중치가 못 붙인 정점을 가장 가까운 변형 뼈(선분)에 붙인다."""
    segments = []
    for bone in armature.data.bones:
        if not bone.use_deform:
            continue
        a = armature.matrix_world @ bone.head_local
        b = armature.matrix_world @ bone.tail_local
        segments.append((bone.name, a, b))
    for i in indices:
        p = mesh.matrix_world @ mesh.data.vertices[i].co
        best, best_d = None, float('inf')
        for name, a, b in segments:
            ab = b - a
            t = 0.0 if ab.length_squared == 0 else max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
            d = (p - (a + ab * t)).length
            if d < best_d:
                best, best_d = name, d
        group = mesh.vertex_groups.get(best) or mesh.vertex_groups.new(name=best)
        group.add([i], 1.0, 'REPLACE')


def fix_skipped_knee(mesh):
    """허벅지(UpLeg)와 발(Foot·ToeBase)에 같이 붙고 종아리(Leg)를 건너뛴 정점을 고친다.

    통바지처럼 다리를 감싼 넓은 메시는 자동 가중치가 바짓단을 허벅지+발에 나눠 붙인다.
    그러면 무릎을 굽힐 때 바짓단이 허벅지 쪽(엉덩이 뒤)에 남아 종아리만 드러난다.
    이런 정점의 허벅지 가중치를 종아리로 옮긴다.
    """
    groups = {g.name: g.index for g in mesh.vertex_groups}
    fixed = 0
    for side in ('Left', 'Right'):
        up = groups.get('mixamorig:%sUpLeg' % side)
        feet = {groups.get('mixamorig:%s%s' % (side, n)) for n in ('Foot', 'ToeBase')} - {None}
        if up is None or not feet:
            continue
        leg = mesh.vertex_groups.get('mixamorig:%sLeg' % side) or mesh.vertex_groups.new(name='mixamorig:%sLeg' % side)
        for v in mesh.data.vertices:
            w = {g.group: g.weight for g in v.groups}
            if w.get(up, 0.0) <= 0.0 or sum(w.get(f, 0.0) for f in feet) < 0.01:
                continue
            leg.add([v.index], w.get(leg.index, 0.0) + w[up], 'REPLACE')
            mesh.vertex_groups[up].remove([v.index])
            fixed += 1
    return fixed


def convert(target):
    spec = TARGETS[target]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    armature, proxy, clips = load_source(spec['source'], target)
    if armature is None or proxy is None:
        print('[%s] 원본 %s의 FBX(With Skin)가 없음' % (target, spec['source']))
        return False
    armature.animation_data.action = None
    armature.data.pose_position = 'REST'
    bpy.context.view_layer.update()

    # 원본 모델 공간: 발 가운데 원점, cm (키 MODEL_HEIGHT)
    plo, phi = mx.world_bounds(proxy)
    base = Vector(((plo.x + phi.x) / 2, (plo.y + phi.y) / 2, plo.z))
    unit = (phi.z - plo.z) / MODEL_HEIGHT  # 월드 / cm

    def to_cm(p):
        return (p - base) / unit

    def to_world(p):
        return base + p * unit

    src = measure_source(armature, to_cm)
    print('[%s] 원본 %s 기준점 %s' % (target, spec['source'], {k: round(v, 1) for k, v in src.items()}))
    map_point = make_mapper(src, spec)

    # 관절 위치 옮기기 (뼈 방향이 크게 안 바뀌므로 회전 키는 그대로 쓴다)
    inv = armature.matrix_world.inverted()
    bpy.ops.object.select_all(action='DESELECT')
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode='EDIT')
    edit = armature.data.edit_bones
    moved = {}
    for bone in edit:
        group = bone_group(bone.name)
        moved[bone.name] = (
            inv @ to_world(map_point(to_cm(armature.matrix_world @ bone.head), group)),
            inv @ to_world(map_point(to_cm(armature.matrix_world @ bone.tail), group)),
        )
    for bone in edit:
        bone.use_connect = False
    for bone in edit:
        roll = bone.roll
        bone.head, bone.tail = moved[bone.name]
        bone.roll = roll
    bpy.ops.object.mode_set(mode='OBJECT')

    # Hips 이동 키(앉을 때 내려가는 거리 등)는 엉덩이 높이 비율만큼 줄이거나 늘린다
    hip_ratio = spec['hip'] / src['hip']
    for _, action in clips:
        for fc in iter_fcurves(action):
            if fc.data_path == 'pose.bones["mixamorig:Hips"].location':
                for k in fc.keyframe_points:
                    k.co[1] *= hip_ratio
                    k.handle_left[1] *= hip_ratio
                    k.handle_right[1] *= hip_ratio

    # 대상 메시를 같은 자리·키로
    work = tempfile.mkdtemp()
    original = mx.import_original(target, work)
    bpy.context.view_layer.update()
    olo, ohi = mx.world_bounds(original)
    original.scale = original.scale * ((phi.z - plo.z) / (ohi.z - olo.z))
    bpy.context.view_layer.update()
    olo, ohi = mx.world_bounds(original)
    original.location += base - Vector(((olo.x + ohi.x) / 2, (olo.y + ohi.y) / 2, olo.z))
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    original.select_set(True)
    bpy.context.view_layer.objects.active = original
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.data.objects.remove(proxy, do_unlink=True)

    unweighted = mx.auto_weights(original, armature, target)
    if unweighted:
        nearest_bone_weights(original, armature, unweighted)
    print('[%s] 무릎을 건너뛴 정점 %d개를 종아리로 옮김' % (target, fix_skipped_knee(original)))

    out_path = mx.export_gltf(target, armature, original, clips)
    shutil.rmtree(work, ignore_errors=True)
    print('GLTF_OK [%s] 클립 %s → %s' % (target, [c for c, _ in clips], out_path))
    return True


def iter_fcurves(action):
    """Blender 4.4+ 레이어 액션과 예전 액션 둘 다에서 F커브를 꺼낸다."""
    if getattr(action, 'layers', None):
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    yield from bag.fcurves
    else:
        yield from action.fcurves


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ok = all(convert(t) for t in (argv or sorted(TARGETS)))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
