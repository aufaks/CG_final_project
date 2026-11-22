"""Mixamo에서 받은 FBX들을 캐릭터마다 glTF 하나(원본 메시 + 뼈대 + 애니메이션 클립 여러 개)로 합친다.

Mixamo에는 저폴리 대용 모델(프록시, tools/make_mixamo_proxy.py)을 올려서 리깅했으므로,
여기서 원본 메시(upload/<캐릭터>.zip: OBJ + 텍스처)를 뼈대에 붙이고 프록시의 뼈 가중치를 가장 가까운 면에서 옮겨 온다.

폴더 구조 (CGFP/CGFP/obj/mixamo 기준):
    upload/<캐릭터>.zip               원본 메시 (tools/prepare_mixamo.py)
    download/<캐릭터>/<클립 이름>.fbx  Mixamo에서 받은 FBX (With Skin). 파일 이름이 곧 게임에서 쓰는 클립 이름
    anim/<캐릭터>.gltf (+ .bin, 텍스처) 결과

실행:
    blender --background --factory-startup --python tools/mixamo_to_gltf.py -- [캐릭터 이름...]
캐릭터 이름을 안 주면 download 아래 폴더 전부를 변환한다.
"""
import math
import os
import shutil
import sys
import tempfile
import zipfile

import bmesh
import bpy
from mathutils import Quaternion, Vector

WELD_DISTANCE = 0.01  # 자동 가중치용 사본에서 이 거리(Blender 단위, 키 약 1.7) 안의 정점을 합친다

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'CGFP', 'obj', 'mixamo')
UPLOAD = os.path.join(ROOT, 'upload')
DOWNLOAD = os.path.join(ROOT, 'download')
OUT = os.path.join(ROOT, 'anim')


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False, ignore_leaf_bones=True)
    return [o for o in bpy.data.objects if o not in before]


def world_bounds(obj):
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def import_original(character, work):
    """원본 OBJ와 텍스처를 읽어 메시 오브젝트 하나로 돌려준다 (텍스처 재질 포함)."""
    with zipfile.ZipFile(os.path.join(UPLOAD, character + '.zip')) as z:
        z.extractall(work)
    before = set(bpy.data.objects)
    # 프록시를 만들 때와 같은 축 설정 (OBJ는 Y-up, 정면 +Z)
    bpy.ops.wm.obj_import(filepath=os.path.join(work, character + '.obj'), forward_axis='NEGATIVE_Z', up_axis='Y')
    obj = next(o for o in bpy.data.objects if o not in before)
    obj.name = character + '_Mesh'

    # 텍스처 재질 (OBJ 재질 대신 확실하게 만든다)
    texture = os.path.join(work, character + '_diffuse.png')
    image = bpy.data.images.load(texture)
    image.pack()  # 임시 폴더를 지워도 내보내기 때 쓸 수 있게
    mat = bpy.data.materials.new(character)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
    tex = nodes.new('ShaderNodeTexImage')
    tex.image = image
    mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = 1.0
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    return obj


def auto_weights(original, armature, character):
    """원본 메시에 뼈 가중치를 붙이고, 자동 가중치가 못 붙인 정점 번호 목록을 돌려준다."""
    # 뼈 가중치: 원본 메시에 Blender 자동 가중치(본 히트)를 계산한다.
    # 저폴리 프록시의 가중치를 옮겨 오면 팔처럼 가는 부위가 팔꿈치 아래까지 위팔 뼈 하나에 붙어 버려서 쓰지 않는다.
    # 자동 가중치가 못 붙인 정점(떨어진 조각 등)만 프록시에서 옮겨 온다.
    # 원본은 UV 경계마다 정점이 쪼개져 있어서 본 히트가 실패한다. 정점을 합친 사본에서 계산해 원본으로 옮긴다.
    welded = original.copy()
    welded.data = original.data.copy()
    bpy.context.collection.objects.link(welded)
    bm = bmesh.new()
    bm.from_mesh(welded.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=WELD_DISTANCE)
    bm.to_mesh(welded.data)
    bm.free()
    armature.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    welded.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    armature.data.pose_position = 'POSE'
    for mod in list(welded.modifiers):
        welded.modifiers.remove(mod)
    welded.parent = None
    welded.matrix_world = original.matrix_world

    for group in welded.vertex_groups:
        original.vertex_groups.new(name=group.name)
    bpy.context.view_layer.objects.active = original
    copy_back = original.modifiers.new('WeldedWeights', 'DATA_TRANSFER')
    copy_back.object = welded
    copy_back.use_vert_data = True
    copy_back.data_types_verts = {'VGROUP_WEIGHTS'}
    copy_back.vert_mapping = 'NEAREST'
    copy_back.layers_vgroup_select_src = 'ALL'
    copy_back.layers_vgroup_select_dst = 'NAME'
    bpy.ops.object.modifier_apply(modifier=copy_back.name)
    bpy.data.objects.remove(welded, do_unlink=True)

    unweighted = [v.index for v in original.data.vertices if not any(g.weight > 1e-4 for g in v.groups)]
    print('[%s] 자동 가중치 없는 정점 %d / %d' % (character, len(unweighted), len(original.data.vertices)))
    return unweighted


# 몸통이 딱딱한 상자인 캐릭터. 자동 가중치가 상자 옆면(눈 옆)을 어깨·팔에, 윗부분을 목·머리에 나눠 붙여서
# 팔을 흔들거나 걸을 때 상자가 찌그러지고 눈 옆이 패인다. 상자 정점은 전부 가슴 뼈(Spine2) 하나에 붙인다.
RIGID_BODY = {'SpongeBob'}


def make_body_rigid(mesh, armature):
    """팔(T포즈 팔 높이 띠의 바깥쪽)과 바지(Spine·Spine1 중간 아래)를 뺀 몸통 정점을 Spine2 하나에 붙인다."""
    def joint(name):
        return armature.matrix_world @ armature.data.bones['mixamorig:' + name].head_local

    hips, arm, head = joint('Hips'), joint('LeftArm'), joint('Head')
    torso = head.z - hips.z
    bottom = (joint('Spine').z + joint('Spine1').z) * 0.5  # 이 아래는 바지·다리
    arm_x = abs(arm.x - hips.x)                            # 어깨 관절 = 몸통 옆면 근처
    band = 0.2 * torso                                     # 팔(소매) 두께의 절반
    body_half = arm_x * 1.35                               # 몸통 옆면 바깥 한계 (이보다 밖은 팔·손)
    spine2 = mesh.vertex_groups.get('mixamorig:Spine2') or mesh.vertex_groups.new(name='mixamorig:Spine2')
    body = []
    for v in mesh.data.vertices:
        p = mesh.matrix_world @ v.co
        dx = abs(p.x - hips.x)
        if p.z < bottom or dx > body_half:
            continue
        if abs(p.z - arm.z) < band and dx > arm_x * 0.95:
            continue  # 소매·팔
        body.append(v.index)
    for group in mesh.vertex_groups:
        if group != spine2:
            group.remove(body)
    spine2.add(body, 1.0, 'REPLACE')
    print('[%s] 몸통 정점 %d개를 Spine2에 붙임' % (armature.name, len(body)))


EAT_CLIP = 'SitEat'
EAT_SECONDS = 3.0
# 먹기 한 번 (진행도 0~1): 접시에서 뜨기 → 입으로 → 입에 댄 채 → 접시로 → 씹으며 다시 뜨기
EAT_KEYS = [  # (진행도, 오른손 위치: 0 = 접시, 1 = 입)
    (0.00, 0.0), (0.08, 0.0), (0.36, 1.0), (0.52, 1.0), (0.78, 0.0), (1.00, 0.0),
]
# 손님들은 팔이 짧아서 손이 입까지 못 간다. 손을 가슴 앞으로 올리고 몸·고개를 숙여 입을 손에 댄다.
EAT_BOW_KEYS = [  # (진행도, 허리 숙이기(도), 고개 숙이기(도)): 손을 맞으러 숙였다가 씹으며 고개 까딱
    (0.00, 0.0, 0.0), (0.30, 0.0, 0.0), (0.42, 18.0, 16.0), (0.52, 18.0, 12.0), (0.66, 0.0, 0.0),
    (0.72, 0.0, 6.0), (0.79, 0.0, 0.0), (0.86, 0.0, 6.0), (0.93, 0.0, 0.0), (1.00, 0.0, 0.0),
]
EAT_BOW_BONES = {'Spine': 0.3, 'Spine1': 0.35, 'Spine2': 0.35}  # 허리 숙이기를 나눠 맡는 뼈 (+X 회전 = 앞으로)


def add_eat_clip(armature, clips):
    """SitIdle 첫 프레임 자세에서 오른손으로 떠먹는 반복 클립(SitEat)을 만들어 clips에 더한다.

    Mixamo에 앉아서 먹는 동작이 없어서 직접 만든다. 접시(테이블 위)·입 위치에 빈 오브젝트를 두고
    팔에 IK를 걸어 손목이 따라가게 한 뒤 키프레임으로 굽는다 (뼈대 길이가 다른 캐릭터에도 맞는다).
    왼손은 테이블 위에 올려 둔다. 몸·고개는 손을 맞으러 숙였다가 씹는 동안 까딱인다.
    """
    sit = dict(clips).get('SitIdle')
    if sit is None or EAT_CLIP in dict(clips):
        return
    scene = bpy.context.scene
    data = armature.animation_data or armature.animation_data_create()
    bones = armature.pose.bones
    armature.data.pose_position = 'POSE'

    # SitIdle 첫 프레임 자세를 고정 자세로 쓴다
    data.action = sit
    if hasattr(data, 'action_slot') and getattr(sit, 'slots', None):
        data.action_slot = sit.slots[0]
    scene.frame_set(int(sit.frame_range[0]))
    static = {pb.name: (pb.location.copy(), pb.rotation_quaternion.copy(), pb.scale.copy()) for pb in bones}
    data.action = None
    for pb in bones:
        pb.rotation_mode = 'QUATERNION'
        pb.location, pb.rotation_quaternion, pb.scale = static[pb.name]
    bpy.context.view_layer.update()

    def world(name, tail=False):
        pb = bones['mixamorig:' + name]
        return armature.matrix_world @ (pb.tail if tail else pb.head)

    rest = armature.data.bones
    foot = armature.matrix_world @ rest['mixamorig:LeftFoot'].head_local
    toe = armature.matrix_world @ rest['mixamorig:LeftToeBase'].head_local
    up = Vector((0.0, 0.0, 1.0))
    forward = Vector((toe.x - foot.x, toe.y - foot.y, 0.0)).normalized()
    side = forward.cross(up)  # 캐릭터 기준 한쪽 옆 (어느 쪽인지는 어깨 위치로 정한다)
    hips, head = world('Hips'), world('Head')
    unit = (head - hips).length  # 엉덩이 → 목 길이. 캐릭터 크기 기준

    def targets(arm):
        shoulder_side = (world(arm) - head).dot(side)
        plate = hips + up * 0.45 * unit + forward * 0.8 * unit + side * shoulder_side * 0.6
        mouth = world(arm) + up * 0.3 * unit + forward * 0.45 * unit - side * shoulder_side * 0.6  # 가슴 앞 가운데
        return plate, mouth

    frames = round(EAT_SECONDS * scene.render.fps)
    start = 0

    def add_target(name, location):
        empty = bpy.data.objects.new(name, None)
        scene.collection.objects.link(empty)
        empty.location = location
        return empty

    def add_ik(bone, target):
        ik = bones['mixamorig:' + bone].constraints.new('IK')
        ik.target = target
        ik.chain_count = 2  # 아래팔 + 위팔
        return ik

    right_plate, right_mouth = targets('RightArm')
    left_plate, _ = targets('LeftArm')
    right = add_target('EatRight', right_plate)
    left = add_target('EatLeft', left_plate - forward * 0.15 * unit)
    for u, k in EAT_KEYS:
        right.location = right_plate.lerp(right_mouth, k)
        right.keyframe_insert('location', frame=start + u * frames)
    add_ik('RightForeArm', right)
    add_ik('LeftForeArm', left)

    # 고정 자세 + 고개 키를 임시 액션에 넣는다
    temp = bpy.data.actions.new('EatTemp')
    data.action = temp
    for pb in bones:
        for path in ('location', 'rotation_quaternion', 'scale'):
            pb.keyframe_insert(path, frame=start)
    bows = [(name, share) for name, share in EAT_BOW_BONES.items() if 'mixamorig:' + name in bones]
    for u, bow, nod in EAT_BOW_KEYS:
        for name, degrees in bows + [('Head', None)]:
            pb = bones['mixamorig:' + name]
            angle = nod if degrees is None else bow * degrees
            pb.rotation_quaternion = static[pb.name][1] @ Quaternion((1.0, 0.0, 0.0), math.radians(angle))
            pb.keyframe_insert('rotation_quaternion', frame=start + u * frames)

    # IK 결과를 키프레임으로 굽는다
    bpy.ops.object.select_all(action='DESELECT')
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.nla.bake(frame_start=start, frame_end=start + frames, step=1, only_selected=False,
                     visual_keying=True, clear_constraints=True, use_current_action=False, bake_types={'POSE'})
    bpy.ops.object.mode_set(mode='OBJECT')
    eat = data.action
    eat.name = EAT_CLIP
    eat.use_fake_user = True
    data.action = None
    bpy.data.actions.remove(temp)
    for empty in (right, left):
        bpy.data.objects.remove(empty, do_unlink=True)
    clips.append((EAT_CLIP, eat))
    print('[%s] %s 클립 %d프레임 만듦' % (armature.name, EAT_CLIP, frames))


def export_gltf(character, armature, original, clips):
    """메시를 뼈대에 붙이고 클립마다 NLA 트랙을 만들어 anim/<캐릭터>.gltf로 내보낸다.
    앉아서 먹는 클립(SitEat)이 없으면 SitIdle로 만들어 더한다."""
    add_eat_clip(armature, clips)
    # 뼈대에 붙이기
    world = original.matrix_world.copy()
    original.parent = armature
    original.matrix_world = world
    mod = original.modifiers.new('Armature', 'ARMATURE')
    mod.object = armature

    # 클립마다 NLA 트랙 하나 → glTF 애니메이션 하나
    data = armature.animation_data or armature.animation_data_create()
    data.action = None
    for track in list(data.nla_tracks):
        data.nla_tracks.remove(track)
    for clip, action in clips:
        track = data.nla_tracks.new()
        track.name = clip
        strip = track.strips.new(clip, int(action.frame_range[0]), action)
        if hasattr(strip, 'action_slot') and getattr(action, 'slots', None):
            strip.action_slot = action.slots[0]

    os.makedirs(OUT, exist_ok=True)
    out_path = os.path.join(OUT, character + '.gltf')
    bpy.ops.object.select_all(action='DESELECT')
    armature.select_set(True)
    original.select_set(True)
    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format='GLTF_SEPARATE',
        use_selection=True,
        export_yup=True,
        export_skins=True,
        export_animations=True,
        export_animation_mode='NLA_TRACKS',
        export_force_sampling=True,
        export_def_bones=True,
        export_texture_dir='',
    )
    return out_path


def convert(character):
    folder = os.path.join(DOWNLOAD, character)
    files = sorted(f for f in os.listdir(folder) if f.lower().endswith('.fbx'))
    if not files:
        print('[%s] FBX 없음' % character)
        return False

    bpy.ops.wm.read_factory_settings(use_empty=True)
    armature = None
    proxy = None
    clips = []
    for name in files:
        clip = os.path.splitext(name)[0]
        objects = import_fbx(os.path.join(folder, name))
        arm = next((o for o in objects if o.type == 'ARMATURE'), None)
        action = arm.animation_data.action if arm and arm.animation_data else None
        if action:
            action.name = clip
            action.use_fake_user = True
            clips.append((clip, action))
        if armature is None and arm is not None:
            armature = arm
            armature.name = character
            proxy = next((o for o in objects if o.type == 'MESH'), None)
            continue
        for o in objects:  # 뼈대·메시는 첫 파일 것만 쓰고 나머지는 애니메이션만 남긴다
            bpy.data.objects.remove(o, do_unlink=True)

    if armature is None or proxy is None:
        print('[%s] 뼈대·메시가 든 FBX(With Skin)가 없음' % character)
        return False
    armature.animation_data.action = None
    bpy.context.view_layer.update()

    # 원본 메시를 프록시 자리에 맞춘다 (같은 모양이라 바운딩 박스로 맞추면 된다)
    work = tempfile.mkdtemp()
    original = import_original(character, work)
    bpy.context.view_layer.update()
    plo, phi = world_bounds(proxy)
    olo, ohi = world_bounds(original)
    scale = (phi.z - plo.z) / (ohi.z - olo.z)
    original.scale = original.scale * scale
    bpy.context.view_layer.update()
    olo, ohi = world_bounds(original)
    pc = Vector(((plo.x + phi.x) / 2, (plo.y + phi.y) / 2, plo.z))
    oc = Vector(((olo.x + ohi.x) / 2, (olo.y + ohi.y) / 2, olo.z))
    original.location += pc - oc
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    original.select_set(True)
    bpy.context.view_layer.objects.active = original
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    unweighted = auto_weights(original, armature, character)
    if unweighted:
        for group in proxy.vertex_groups:
            if group.name not in original.vertex_groups:
                original.vertex_groups.new(name=group.name)
        mask = original.vertex_groups.new(name='_Unweighted')
        mask.add(unweighted, 1.0, 'REPLACE')
        transfer = original.modifiers.new('WeightTransfer', 'DATA_TRANSFER')
        transfer.object = proxy
        transfer.use_vert_data = True
        transfer.data_types_verts = {'VGROUP_WEIGHTS'}
        transfer.vert_mapping = 'POLYINTERP_NEAREST'
        transfer.layers_vgroup_select_src = 'ALL'
        transfer.layers_vgroup_select_dst = 'NAME'
        transfer.vertex_group = mask.name
        bpy.context.view_layer.objects.active = original
        bpy.ops.object.modifier_move_to_index(modifier=transfer.name, index=0)
        bpy.ops.object.modifier_apply(modifier=transfer.name)
        original.vertex_groups.remove(original.vertex_groups['_Unweighted'])
    bpy.data.objects.remove(proxy, do_unlink=True)
    if character in RIGID_BODY:
        make_body_rigid(original, armature)

    out_path = export_gltf(character, armature, original, clips)
    shutil.rmtree(work, ignore_errors=True)
    print('GLTF_OK [%s] 클립 %s → %s' % (character, [c for c, _ in clips], out_path))
    return True


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    characters = argv or sorted(d for d in os.listdir(DOWNLOAD) if os.path.isdir(os.path.join(DOWNLOAD, d)))
    ok = all(convert(c) for c in characters)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
