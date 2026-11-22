"""Mixamo 자동 리깅용 저폴리 대용 모델(프록시)을 만든다.

Mixamo에는 뼈대를 심을 모양만 있으면 되므로, upload/<캐릭터>.zip의 OBJ를 줄여서
upload/proxy/<캐릭터>_proxy.zip(OBJ 하나, UV·노멀 없음)으로 만든다.
원본 메시·UV·텍스처는 tools/mixamo_to_gltf.py가 받아 온 뼈대에 다시 입힌다 (프록시에서 가중치를 옮긴다).

실행:
    blender --background --factory-startup --python tools/make_mixamo_proxy.py
"""
import os
import shutil
import sys
import tempfile
import zipfile

import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'CGFP', 'obj', 'mixamo', 'upload')
OUT = os.path.join(ROOT, 'proxy')
TARGET_FACES = 1200
CHARACTERS = ['FishG', 'FishH', 'Fred', 'Morty', 'SpongeBob', 'Squidward']
THIN_PARTS = {'Fred': 4.0}  # 캐릭터 → 메시 전체에 줄 두께(cm)
# 캐릭터별 목표 면 수. 징징이처럼 원래 가늘고 면이 적은 모델은 줄이면 다리·목이 뭉개져 Mixamo가 실패한다
TARGET_FACES_BY_CHARACTER = {'Squidward': 600}
# 메시가 열려 있거나 겹친 면이 많아 Mixamo가 "Unknown error"를 내는 캐릭터는 복셀로 다시 짜서 닫힌 메시로 만든다 (복셀 크기 cm)
VOXEL_REMESH = {'Squidward': 1.5}


def make(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    work = tempfile.mkdtemp()
    with zipfile.ZipFile(os.path.join(ROOT, name + '.zip')) as z:
        z.extractall(work)
    # 원본 좌표 그대로 (Y-up, cm) 읽고 내보내도록 축 변환을 서로 맞춘다
    bpy.ops.wm.obj_import(filepath=os.path.join(work, name + '.obj'), forward_axis='NEGATIVE_Z', up_axis='Y')
    obj = bpy.context.selected_objects[0]
    bpy.context.view_layer.objects.active = obj
    # 같은 위치 정점을 합쳐야 줄이기가 제대로 된다 (OBJ는 UV 경계에서 정점이 갈라져 있다)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=0.01)
    bpy.ops.object.mode_set(mode='OBJECT')
    if name in VOXEL_REMESH:
        mod = obj.modifiers.new('Remesh', 'REMESH')
        mod.mode = 'VOXEL'
        mod.voxel_size = VOXEL_REMESH[name]
        bpy.ops.object.modifier_apply(modifier=mod.name)
    faces = len(obj.data.polygons)
    target = TARGET_FACES_BY_CHARACTER.get(name, TARGET_FACES)
    if faces > target:
        mod = obj.modifiers.new('Decimate', 'DECIMATE')
        mod.ratio = target / faces
        bpy.ops.object.modifier_apply(modifier=mod.name)
    if name in THIN_PARTS:
        # 두께가 없는 판(Fred의 지느러미 팔)은 자동 리깅이 팔로 인식하지 못하므로 두께를 준다
        mod = obj.modifiers.new('Solidify', 'SOLIDIFY')
        mod.thickness = THIN_PARTS[name]
        mod.offset = 0.0
        bpy.ops.object.modifier_apply(modifier=mod.name)
    # 줄이면서 생긴 찌그러진 면을 정리하고 노멀 방향을 맞춘다
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.dissolve_degenerate(threshold=0.05)
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    os.makedirs(OUT, exist_ok=True)
    obj_path = os.path.join(work, name + '_proxy.obj')
    bpy.ops.wm.obj_export(filepath=obj_path, forward_axis='NEGATIVE_Z', up_axis='Y', export_uv=False,
                          export_normals=False, export_materials=False, apply_modifiers=True)
    # 좌표는 1mm 단위면 리깅에 충분하다 (파일이 작아진다)
    lines = []
    with open(obj_path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('v '):
                lines.append('v ' + ' '.join(('%.1f' % float(x)).rstrip('0').rstrip('.') for x in line.split()[1:4]) + '\n')
            elif line.startswith('f '):
                lines.append('f ' + ' '.join(t.split('/')[0] for t in line.split()[1:]) + '\n')
    zip_path = os.path.join(OUT, name + '_proxy.zip')
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.writestr(name + '_proxy.obj', ''.join(lines))
    shutil.rmtree(work, ignore_errors=True)
    print('PROXY %s: %d -> %d faces, %d bytes' % (name, faces, len(obj.data.polygons), os.path.getsize(zip_path)))


for c in (sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else CHARACTERS):
    make(c)
