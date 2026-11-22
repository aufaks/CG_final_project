"""손님 모델을 Mixamo 업로드용 zip(OBJ + MTL + 텍스처)으로 정리한다.

- 발 중심을 원점에, 정면을 +Z로 맞춘다 (Morty는 원본 정면이 -Z라 180도 돌린다)
- Fish_G·Fish_H의 T포즈 손 바깥에 떠 있는 판 조각을 잘라 낸다 (Mixamo 자동 리깅이 손 위치를 헷갈린다)
- 키를 약 170 단위(cm)로 맞춘다. Mixamo는 사람 크기 모델에서 마커가 가장 잘 잡힌다.

실행 (프로젝트 CGFP/CGFP 폴더 기준 경로를 쓴다):
    python tools/prepare_mixamo.py
결과: CGFP/obj/mixamo/upload/<이름>.zip
"""
import os
import shutil
import zipfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'CGFP')
OUT = os.path.join(ROOT, 'obj', 'mixamo', 'upload')
TARGET_HEIGHT = 170.0

MODELS = [
    # 이름, OBJ, 텍스처, 정면이 -Z인가, 잘라 낼 바깥 x(키 대비, 0이면 안 자름)
    ('FishG', 'obj/imported/sbfbbr-fish-g/Fish_G.obj', 'obj/imported/sbfbbr-fish-g/T_Fish_G_D.png', False, 0.31),
    ('FishH', 'obj/imported/sbfbbr-fish-h/Fish_H.obj', 'obj/imported/sbfbbr-fish-h/T_Fish_H_D.png', False, 0.38),
    ('Fred', 'obj/viewer_models/Fred.obj', 'obj/viewer_models/fish.png', False, 0.0),
    ('Morty', 'obj/viewer_models/Morty.obj', 'obj/viewer_models/Meshesmorty1_diff.png', True, 0.0),
    # 플레이어·징징이 (둘 다 T포즈, 정면 +Z)
    ('SpongeBob', 'obj/imported/spongebob-squarepants/spongebob_bind.obj', 'obj/imported/spongebob-squarepants/textures/spongebob.png', False, 0.0),
    ('Squidward', 'obj/imported/squidwards-house/Mesh_0037.rip.obj', 'obj/imported/squidwards-house/t0077_0.png', False, 0.0),
]


def read_obj(path):
    v, vt, vn, faces = [], [], [], []
    with open(path, encoding='utf-8', errors='ignore') as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == 'v':
                v.append([float(x) for x in p[1:4]])
            elif p[0] == 'vt':
                vt.append([float(x) for x in p[1:3]])
            elif p[0] == 'vn':
                vn.append([float(x) for x in p[1:4]])
            elif p[0] == 'f':
                face = []
                for tok in p[1:]:
                    parts = tok.split('/')
                    idx = [int(parts[0]) if parts[0] else 0,
                           int(parts[1]) if len(parts) > 1 and parts[1] else 0,
                           int(parts[2]) if len(parts) > 2 and parts[2] else 0]
                    # 음수 인덱스 → 양수
                    counts = (len(v), len(vt), len(vn))
                    idx = [i if i >= 0 else counts[k] + 1 + i for k, i in enumerate(idx)]
                    face.append(idx)
                faces.append(face)
    return v, vt, vn, faces


def convert(name, obj, tex, flip, drop):
    v, vt, vn, faces = read_obj(os.path.join(ROOT, obj))
    if flip:
        v = [[-x, y, -z] for x, y, z in v]
        vn = [[-x, y, -z] for x, y, z in vn]
    xs = [p[0] for p in v]
    ys = [p[1] for p in v]
    height = max(ys) - min(ys)
    cx = (min(xs) + max(xs)) / 2
    if drop > 0:
        faces = [f for f in faces if any(abs(v[i[0] - 1][0] - cx) <= drop * height for i in f)]

    used = sorted({i[0] for f in faces for i in f})
    pts = [v[i - 1] for i in used]
    minx = min(p[0] for p in pts); maxx = max(p[0] for p in pts)
    miny = min(p[1] for p in pts); maxy = max(p[1] for p in pts)
    minz = min(p[2] for p in pts); maxz = max(p[2] for p in pts)
    pivot = ((minx + maxx) / 2, miny, (minz + maxz) / 2)
    scale = TARGET_HEIGHT / (maxy - miny)

    os.makedirs(OUT, exist_ok=True)
    work = os.path.join(OUT, name)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    tex_name = name + '_diffuse.png'
    shutil.copyfile(os.path.join(ROOT, tex), os.path.join(work, tex_name))
    with open(os.path.join(work, name + '.mtl'), 'w', encoding='utf-8') as f:
        f.write('newmtl %s\nKd 1 1 1\nmap_Kd %s\n' % (name, tex_name))
    with open(os.path.join(work, name + '.obj'), 'w', encoding='utf-8') as f:
        f.write('mtllib %s.mtl\no %s\n' % (name, name))
        for x, y, z in v:
            f.write('v %.6f %.6f %.6f\n' % ((x - pivot[0]) * scale, (y - pivot[1]) * scale, (z - pivot[2]) * scale))
        for a, b in vt:
            f.write('vt %.6f %.6f\n' % (a, b))
        for a, b, c in vn:
            f.write('vn %.6f %.6f %.6f\n' % (a, b, c))
        f.write('usemtl %s\n' % name)
        for face in faces:
            toks = []
            for i, t, n in face:
                tok = str(i)
                if t or n:
                    tok += '/' + (str(t) if t else '')
                if n:
                    tok += '/' + str(n)
                toks.append(tok)
            f.write('f ' + ' '.join(toks) + '\n')

    zip_path = os.path.join(OUT, name + '.zip')
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
        for fn in os.listdir(work):
            z.write(os.path.join(work, fn), fn)
    shutil.rmtree(work)
    print('%s: %d faces -> %s' % (name, len(faces), zip_path))


if __name__ == '__main__':
    for m in MODELS:
        convert(*m)
