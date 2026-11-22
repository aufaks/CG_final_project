"""징징이 모델 + 뼈대 + 애니메이션 클립을 glTF 하나(anim/Squidward.gltf)로 굽는다.

징징이 동작(한숨, 주문 받기, 영수증 넣기)은 Mixamo에 없는 동작이라 직접 만든다.
원래 Character/Squidward.cpp가 매 프레임 계산하던 자세 곡선을 그대로 옮겨 30fps 키프레임으로 저장한다.
게임은 손님·플레이어와 똑같이 SkinnedModel + Animator로 읽어 재생한다.
결과 파일은 Blender에서 열어(File > Import > glTF) 키프레임을 고친 뒤 다시 내보내도 된다.

뼈대 (모델 좌표: 발 가운데가 원점, Y-up, 정면 +Z, 원본 OBJ 단위)
    Root          발. 다리 부위가 붙고, SubmitOrder의 뒤돌기(루트 회전)가 여기 들어간다
      Torso       허리에서 숙임, 숨쉬기 들썩임
        Head      목에서 끄덕임·갸웃
        RightArm  오른쪽(-X) 어깨
          RightHand  오른손 끝 (가중치 없음, 영수증을 붙이는 위치)
        LeftArm   왼쪽(+X) 어깨

클립: Idle(반복, 8초), Sigh(4.8초), TakeOrder(3.2초), SubmitOrder(3.8초)

실행 (CGFP 폴더, tools가 있는 곳에서):
    python tools/squidward_to_gltf.py
결과: CGFP/obj/mixamo/anim/Squidward.gltf, Squidward.bin, Squidward_diffuse.png
"""
import json
import math
import os
import shutil
import struct

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'CGFP')
OBJ = os.path.join(ROOT, 'obj', 'imported', 'squidwards-house', 'Mesh_0037.rip.obj')
TEXTURE = os.path.join(ROOT, 'obj', 'imported', 'squidwards-house', 't0077_0.png')
OUT = os.path.join(ROOT, 'obj', 'mixamo', 'anim')
NAME = 'Squidward'
FPS = 30

# 게임에서 징징이 키(월드)와 창구까지 걷는 거리(월드). 루트 모션 거리를 모델 단위로 바꿀 때 쓴다
GAME_HEIGHT = 1.6
WINDOW_STEP = 0.55

# ---- 관절 위치 (키 대비 높이). OBJ는 T포즈: 팔(소매 포함)은 0.47~0.63 높이에서 좌우로 뻗어 있다.
LEG_TOP = 0.30      # 허리(셔츠 아랫단 근처)
NECK = 0.615        # 목
ARM_BAND_LOW = 0.47
ARM_BAND_HIGH = 0.63
JOINT_BLEND = 0.03  # 허리·목에서 위아래 부위를 섞는 폭 (키 대비)
SHOULDER_X = 0.20   # 어깨: 몸통 옆면 (OBJ 단위). 소매는 팔 쪽에 넣는다
SHOULDER_BLEND = 0.07
ARM_DROP = 68.0     # T포즈 팔을 내린 기본 자세 (90이면 완전히 수직)

# ---- Idle: 심드렁하게 조금씩 흔들린다. 8초 안에 숨 2번, 고개 1번 왕복해서 이음매 없이 반복된다
IDLE_SECONDS = 8.0
BREATH_SPEED = 2.0 * math.pi * 2.0 / IDLE_SECONDS
BREATH_HEIGHT = 0.006   # 키 대비
HEAD_SWAY = 2.5
HEAD_SWAY_SPEED = 2.0 * math.pi / IDLE_SECONDS

# ---- Sigh: 숨 들이쉬기 → 잠깐 멈춤 → 한숨 쉬며 축 처짐 → 원래대로
SIGH_SECONDS = 4.8
INHALE_LIFT = 0.018
INHALE_HEAD_BACK = -8.0
INHALE_SHRUG = -10.0
SIGH_SLUMP = 7.0
SIGH_DROP = 0.025
SIGH_HEAD_DROOP = 20.0
SIGH_HEAD_TILT = 6.0
SIGH_ARM_HANG = 8.0

# ---- TakeOrder: 손님 쪽으로 기울기 → 고개 끄덕이며 듣기 → 왼손 메모지에 오른손으로 적기 → 영수증 뜯기
TAKE_ORDER_SECONDS = 3.2
ORDER_LEAN = 6.0
ORDER_NOD = 9.0
PAD_ARM_FORWARD = 62.0
PAD_ARM_DROP = 52.0
PEN_ARM_FORWARD = 52.0
PEN_ARM_DROP = 58.0
SCRIBBLE = 6.0
LOOK_AT_PAD = 14.0
TEAR_UP = -30.0

# ---- SubmitOrder: 제자리에서 뒤돌기 → 팔을 뻗어 영수증 꽂기 → 팔 내리기 → 다시 돌기 (걷지 않음)
SUBMIT_SECONDS = 3.8
REACH_FORWARD = 88.0
REACH_PITCH = -38.0
REACH_LEAN = 5.0

BONES = ['Root', 'Torso', 'Head', 'RightArm', 'LeftArm']  # 스킨 뼈 순서
LEGS, TORSO, HEAD, RARM, LARM = range(5)


# ---------------------------------------------------------------- 수학
def smooth(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def quat_axis(axis, degrees):
    """(x, y, z, w)"""
    h = math.radians(degrees) * 0.5
    s = math.sin(h)
    return np.array([axis[0] * s, axis[1] * s, axis[2] * s, math.cos(h)])


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    ])


def quat_rotate(q, v):
    u = q[:3]
    w = q[3]
    return 2.0 * np.dot(u, v) * u + (w * w - np.dot(u, u)) * v + 2.0 * w * np.cross(u, v)


X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def qx(d): return quat_axis(X, d)
def qy(d): return quat_axis(Y, d)
def qz(d): return quat_axis(Z, d)


# ---------------------------------------------------------------- OBJ
def read_obj(path):
    v, vt, vn = [], [], []
    verts, index = [], {}
    tris = []
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
                    parts = (tok.split('/') + ['', ''])[:3]
                    key = []
                    for k, (s, n) in enumerate(zip(parts, (len(v), len(vt), len(vn)))):
                        i = int(s) if s else 0
                        key.append(i - 1 if i > 0 else (n + i if i < 0 else -1))
                    key = tuple(key)
                    if key not in index:
                        index[key] = len(verts)
                        verts.append(key)
                    face.append(index[key])
                for i in range(1, len(face) - 1):
                    tris.append((face[0], face[i], face[i + 1]))
    pos = np.array([v[a] for a, _, _ in verts], dtype=np.float64)
    uv = np.array([vt[b] if b >= 0 else [0.0, 0.0] for _, b, _ in verts], dtype=np.float64)
    nor = np.array([vn[c] if c >= 0 else [0.0, 1.0, 0.0] for _, _, c in verts], dtype=np.float64)
    return pos, nor, uv, np.array(tris, dtype=np.uint32)


# ---------------------------------------------------------------- 뼈대·가중치
def build_rig(pos):
    lo, hi = pos.min(axis=0), pos.max(axis=0)
    h = hi[1] - lo[1]
    leg_top = h * LEG_TOP
    neck_y = h * NECK
    arm_low = h * ARM_BAND_LOW
    arm_high = h * ARM_BAND_HIGH
    blend = h * JOINT_BLEND

    side = np.abs(pos[:, 0])
    arm_mask = (pos[:, 1] >= arm_low) & (pos[:, 1] <= arm_high) & (side > SHOULDER_X + SHOULDER_BLEND)
    torso_mask = (pos[:, 1] >= leg_top) & (pos[:, 1] <= arm_low)
    arm_y = pos[arm_mask, 1].mean() if arm_mask.any() else (arm_low + arm_high) * 0.5
    arm_z = pos[arm_mask, 2].mean() if arm_mask.any() else 0.0
    torso_z = pos[torso_mask, 2].mean() if torso_mask.any() else 0.0
    rig = {
        'height': h,
        'waist': np.array([0.0, leg_top, torso_z]),
        'neck': np.array([0.0, neck_y, torso_z]),
        'shoulder_r': np.array([-SHOULDER_X, arm_y, arm_z]),
        'shoulder_l': np.array([SHOULDER_X, arm_y, arm_z]),
    }

    # 정점 가중치. 같은 위치의 정점은 같은 가중치를 받으므로 UV 경계에서 메시가 벌어지지 않는다.
    w = np.zeros((len(pos), 5))
    for i, p in enumerate(pos):
        s = abs(p[0])
        in_band = smooth(arm_low - blend, arm_low + blend, p[1]) * (1.0 - smooth(arm_high - blend, arm_high + blend, p[1]))
        arm = in_band * smooth(SHOULDER_X - SHOULDER_BLEND, SHOULDER_X + SHOULDER_BLEND, s)
        head = smooth(neck_y - blend, neck_y + blend, p[1])
        upper = smooth(leg_top - blend, leg_top + blend, p[1])
        w[i, RARM if p[0] < 0 else LARM] = arm
        rest = 1.0 - arm
        w[i, HEAD] = rest * head
        w[i, TORSO] = rest * (1.0 - head) * upper
        w[i, LEGS] = rest * (1.0 - head) * (1.0 - upper)

    # 오른손 끝: 오른팔(-X) 정점 중 가장 바깥쪽 근처의 중심
    right = w[:, RARM] > 0.9
    if right.any():
        tip = pos[right, 0].min()
        hand = right & (pos[:, 0] < tip + h * 0.03)
        rig['hand_r'] = pos[hand].mean(axis=0)
    else:
        rig['hand_r'] = rig['shoulder_r'].copy()
    return rig, w


# ---------------------------------------------------------------- 자세 곡선
def base_pose(t):
    return {
        'lift': BREATH_HEIGHT * (0.5 + 0.5 * math.sin(t * BREATH_SPEED)),
        'lean': 0.0, 'headPitch': 0.0,
        'headRoll': HEAD_SWAY * math.sin(t * HEAD_SWAY_SPEED),
        'armDrop': [ARM_DROP, ARM_DROP], 'armForward': [0.0, 0.0], 'armPitch': [0.0, 0.0],
        'turn': 0.0, 'step': 0.0,
    }


def pose_idle(t):
    return base_pose(t)


def pose_sigh(t):
    p = base_pose(t)
    u = t / SIGH_SECONDS
    inhale = smooth(0.0, 0.29, u) * (1.0 - smooth(0.33, 0.46, u))
    sigh = smooth(0.38, 0.58, u) * (1.0 - smooth(0.71, 1.0, u))
    p['lift'] += INHALE_LIFT * inhale - SIGH_DROP * sigh
    p['lean'] = SIGH_SLUMP * sigh
    p['headPitch'] = INHALE_HEAD_BACK * inhale + SIGH_HEAD_DROOP * sigh
    p['headRoll'] += SIGH_HEAD_TILT * sigh
    p['armDrop'] = [d + INHALE_SHRUG * inhale + SIGH_ARM_HANG * sigh for d in p['armDrop']]
    return p


def pose_take_order(t):
    p = base_pose(t)
    u = t / TAKE_ORDER_SECONDS
    ready = smooth(0.0, 0.15, u) * (1.0 - smooth(0.9, 1.0, u))
    listen = 0.5 - 0.5 * math.cos((u - 0.1) / 0.2 * 4.0 * math.pi) if 0.1 < u < 0.3 else 0.0  # 고개 두 번 끄덕
    write = smooth(0.28, 0.34, u) * (1.0 - smooth(0.72, 0.78, u))
    tear = smooth(0.78, 0.84, u) * (1.0 - smooth(0.86, 0.97, u))
    scribble = write * math.sin(t * 22.0)
    scribble2 = write * math.sin(t * 15.0 + 1.0)
    p['lean'] = ORDER_LEAN * ready
    p['headPitch'] = ORDER_NOD * listen + LOOK_AT_PAD * write
    p['armDrop'][1] = ARM_DROP + (PAD_ARM_DROP - ARM_DROP) * ready   # 왼팔: 메모지
    p['armForward'][1] = PAD_ARM_FORWARD * ready
    p['armDrop'][0] = ARM_DROP + (PEN_ARM_DROP - ARM_DROP) * ready   # 오른팔: 펜 → 영수증 뜯기
    p['armForward'][0] = PEN_ARM_FORWARD * ready + SCRIBBLE * scribble
    p['armPitch'][0] = SCRIBBLE * 0.6 * scribble2 + TEAR_UP * tear
    return p


def pose_submit_order(t):
    p = base_pose(t)
    u = t / SUBMIT_SECONDS
    reach = smooth(0.29, 0.42, u) * (1.0 - smooth(0.52, 0.66, u))
    push = smooth(0.40, 0.46, u) * (1.0 - smooth(0.46, 0.52, u))  # 영수증을 넣는 순간 손을 살짝 밀어 넣는다
    p['lean'] = REACH_LEAN * reach + 3.0 * push
    p['headPitch'] = -6.0 * reach  # 창구를 올려다본다
    p['armDrop'][0] = ARM_DROP * (1.0 - reach)
    p['armForward'][0] = REACH_FORWARD * reach
    p['armPitch'][0] = REACH_PITCH * reach - 8.0 * push
    p['turn'] = smooth(0.0, 0.18, u) - smooth(0.82, 1.0, u)
    return p  # 제자리에서 돌기만 한다 (step = 0)


CLIPS = [
    ('Idle', IDLE_SECONDS, pose_idle),
    ('Sigh', SIGH_SECONDS, pose_sigh),
    ('TakeOrder', TAKE_ORDER_SECONDS, pose_take_order),
    ('SubmitOrder', SUBMIT_SECONDS, pose_submit_order),
]


def node_pose(rig, p):
    """자세 → 노드별 (translation, rotation). 부모 쪽에서 본 로컬 값."""
    h = rig['height']
    step = WINDOW_STEP / GAME_HEIGHT * h
    lean = qx(p['lean'])
    out = {
        # 뒤로 돌아(+Y 회전) 원래 정면의 반대(-Z) 쪽으로 걸어간다
        'Root': (np.array([0.0, 0.0, -step * p['step']]), qy(180.0 * p['turn'])),
        'Torso': (rig['waist'] + quat_rotate(lean, np.array([0.0, p['lift'] * h, 0.0])), lean),
        'Head': (rig['neck'] - rig['waist'], quat_mul(qx(p['headPitch']), qz(p['headRoll']))),
        # -X 팔: Z축 +회전이 아래로, Y축 +회전이 앞(+Z)으로, X축 -회전이 위로. +X 팔은 Y·Z 부호가 반대.
        'RightArm': (rig['shoulder_r'] - rig['waist'],
                     quat_mul(quat_mul(qx(p['armPitch'][0]), qy(p['armForward'][0])), qz(p['armDrop'][0]))),
        'LeftArm': (rig['shoulder_l'] - rig['waist'],
                    quat_mul(quat_mul(qx(p['armPitch'][1]), qy(-p['armForward'][1])), qz(-p['armDrop'][1]))),
    }
    return out


# ---------------------------------------------------------------- glTF 쓰기
class Writer:
    def __init__(self):
        self.bin = bytearray()
        self.views = []
        self.accessors = []

    def add(self, array, component, type_, target=None, minmax=False):
        array = np.ascontiguousarray(array)
        while len(self.bin) % 4:
            self.bin.append(0)
        offset = len(self.bin)
        self.bin += array.tobytes()
        view = {'buffer': 0, 'byteOffset': offset, 'byteLength': array.nbytes}
        if target:
            view['target'] = target
        self.views.append(view)
        acc = {'bufferView': len(self.views) - 1, 'componentType': component,
               'count': int(array.shape[0]), 'type': type_}
        if minmax:
            flat = array.reshape(array.shape[0], -1)
            acc['min'] = [float(x) for x in flat.min(axis=0)]
            acc['max'] = [float(x) for x in flat.max(axis=0)]
        self.accessors.append(acc)
        return len(self.accessors) - 1


FLOAT, USHORT, UINT = 5126, 5123, 5125
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963


def main():
    pos, nor, uv, tris = read_obj(OBJ)
    # 발 가운데를 원점으로
    lo, hi = pos.min(axis=0), pos.max(axis=0)
    pivot = np.array([(lo[0] + hi[0]) * 0.5, lo[1], (lo[2] + hi[2]) * 0.5])
    pos = pos - pivot
    rig, weights = build_rig(pos)

    # 뼈 4개까지 (팔 하나 + 머리·몸통·다리라서 4개를 넘지 않는다)
    order = np.argsort(-weights, axis=1)[:, :4]
    joints = order.astype(np.uint16)
    w4 = np.take_along_axis(weights, order, axis=1)
    w4 = w4 / w4.sum(axis=1, keepdims=True)

    rest = {  # 바인드 자세의 노드 전역 위치
        'Root': np.zeros(3), 'Torso': rig['waist'], 'Head': rig['neck'],
        'RightArm': rig['shoulder_r'], 'LeftArm': rig['shoulder_l'],
    }
    names = BONES + ['RightHand']
    parent = {'Torso': 'Root', 'Head': 'Torso', 'RightArm': 'Torso', 'LeftArm': 'Torso', 'RightHand': 'RightArm'}
    rest_local = {n: rest[n] - (rest[parent[n]] if n in parent else 0) for n in BONES}
    rest_local['RightHand'] = rig['hand_r'] - rig['shoulder_r']

    wr = Writer()
    a_pos = wr.add(pos.astype(np.float32), FLOAT, 'VEC3', ARRAY_BUFFER, minmax=True)
    a_nor = wr.add(nor.astype(np.float32), FLOAT, 'VEC3', ARRAY_BUFFER)
    # glTF UV는 이미지 위쪽이 v=0 (OBJ는 아래쪽)
    a_uv = wr.add(np.stack([uv[:, 0], 1.0 - uv[:, 1]], axis=1).astype(np.float32), FLOAT, 'VEC2', ARRAY_BUFFER)
    a_joint = wr.add(joints, USHORT, 'VEC4', ARRAY_BUFFER)
    a_weight = wr.add(w4.astype(np.float32), FLOAT, 'VEC4', ARRAY_BUFFER)
    a_index = wr.add(tris.reshape(-1).astype(np.uint32), UINT, 'SCALAR', ELEMENT_ARRAY_BUFFER)
    inverse_bind = []
    for n in BONES:
        m = np.identity(4)
        m[:3, 3] = -rest[n]
        inverse_bind.append(m.T.reshape(-1))  # 열 우선
    a_ibm = wr.add(np.array(inverse_bind, dtype=np.float32), FLOAT, 'MAT4')

    animations = []
    for clip, seconds, fn in CLIPS:
        frames = int(round(seconds * FPS))
        times = np.array([i / FPS for i in range(frames + 1)], dtype=np.float32)
        times[-1] = seconds
        tracks = {n: ([], []) for n in BONES}
        for t in times:
            for n, (tr, q) in node_pose(rig, fn(float(t))).items():
                rots = tracks[n][1]
                if rots and np.dot(rots[-1], q) < 0.0:  # 이웃 키와 같은 반구 (보간이 먼 길로 돌지 않게)
                    q = -q
                tracks[n][0].append(tr)
                rots.append(q)
        a_time = wr.add(times, FLOAT, 'SCALAR', minmax=True)
        samplers, channels = [], []
        for n in BONES:
            for path, values in (('translation', tracks[n][0]), ('rotation', tracks[n][1])):
                out = wr.add(np.array(values, dtype=np.float32), FLOAT, 'VEC3' if path == 'translation' else 'VEC4')
                samplers.append({'input': a_time, 'output': out, 'interpolation': 'LINEAR'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': names.index(n), 'path': path}})
        animations.append({'name': clip, 'samplers': samplers, 'channels': channels})

    nodes = []
    for n in names:
        node = {'name': n, 'translation': [float(x) for x in rest_local[n]]}
        children = [names.index(c) for c, p in parent.items() if p == n]
        if children:
            node['children'] = children
        nodes.append(node)
    mesh_node = len(nodes)
    nodes.append({'name': NAME + '_Mesh', 'mesh': 0, 'skin': 0})

    os.makedirs(OUT, exist_ok=True)
    texture_name = NAME + '_diffuse.png'
    shutil.copyfile(TEXTURE, os.path.join(OUT, texture_name))
    gltf = {
        'asset': {'version': '2.0', 'generator': 'tools/squidward_to_gltf.py'},
        'scene': 0,
        'scenes': [{'name': NAME, 'nodes': [0, mesh_node]}],
        'nodes': nodes,
        'skins': [{'name': NAME, 'joints': [names.index(n) for n in BONES], 'skeleton': 0, 'inverseBindMatrices': a_ibm}],
        'meshes': [{'name': NAME, 'primitives': [{
            'attributes': {'POSITION': a_pos, 'NORMAL': a_nor, 'TEXCOORD_0': a_uv, 'JOINTS_0': a_joint, 'WEIGHTS_0': a_weight},
            'indices': a_index, 'material': 0}]}],
        'materials': [{'name': NAME, 'pbrMetallicRoughness': {
            'baseColorTexture': {'index': 0}, 'metallicFactor': 0.0, 'roughnessFactor': 1.0}}],
        'textures': [{'sampler': 0, 'source': 0}],
        'samplers': [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}],
        'images': [{'uri': texture_name}],
        'animations': animations,
        'buffers': [{'uri': NAME + '.bin', 'byteLength': len(wr.bin)}],
        'bufferViews': wr.views,
        'accessors': wr.accessors,
    }
    with open(os.path.join(OUT, NAME + '.bin'), 'wb') as f:
        f.write(wr.bin)
    with open(os.path.join(OUT, NAME + '.gltf'), 'w', encoding='utf-8') as f:
        json.dump(gltf, f, indent=1)
    print('GLTF_OK %s: 정점 %d, 삼각형 %d, 클립 %s -> %s' % (
        NAME, len(pos), len(tris), [c for c, _, _ in CLIPS], os.path.join(OUT, NAME + '.gltf')))


if __name__ == '__main__':
    main()
