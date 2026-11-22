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
import os
import shutil
import sys
import tempfile
import zipfile

import bmesh
import bpy
from mathutils import Vector

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


def export_gltf(character, armature, original, clips):
    """메시를 뼈대에 붙이고 클립마다 NLA 트랙을 만들어 anim/<캐릭터>.gltf로 내보낸다."""
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
