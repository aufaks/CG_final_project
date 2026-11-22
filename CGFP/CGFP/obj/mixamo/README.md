# 손님 애니메이션 (Mixamo → Blender → glTF)

게임의 손님 애니메이션은 코드가 아니라 이 폴더의 glTF 파일에서 읽는다.

```
upload/<캐릭터>.zip          Mixamo에 올리는 모델 (tools/prepare_mixamo.py가 만든다)
download/<캐릭터>/<클립>.fbx  Mixamo에서 받은 FBX. 파일 이름 = 게임에서 쓰는 클립 이름
anim/<캐릭터>.gltf (+.bin, 텍스처)  게임이 읽는 결과 (tools/mixamo_to_gltf.py가 만든다)
```

캐릭터: `FishG`, `FishH`, `Fred`, `Morty`

## 1. Mixamo에서 리깅

1. https://www.mixamo.com 로그인 (Adobe 무료 계정)
2. **Upload Character** → `upload/<캐릭터>.zip`
3. 마커 위치: 턱(Chin), 손목(Wrists), 팔꿈치(Elbows), 무릎(Knees), 사타구니(Groin)
4. **Skeleton LOD: No Fingers (25)** — 게임은 뼈를 최대 64개까지 받는다

## 2. 애니메이션 받기

캐릭터마다 아래 4개를 받는다. 저장할 때 파일 이름을 클립 이름으로 바꾼다.

| 클립 이름 | Mixamo 검색어 | 설정 |
| --- | --- | --- |
| `Idle.fbx` | Idle (서서 숨쉬기) | — |
| `Walk.fbx` | Walking | **In Place 체크** |
| `StandToSit.fbx` | Stand To Sit | — |
| `SitIdle.fbx` | Sitting Idle | — |

다운로드 설정: **Format FBX Binary(.fbx), Skin: With Skin, Frames per Second 30, Keyframe Reduction none**

저장 위치: `download/<캐릭터>/` (예: `download/FishG/Walk.fbx`)

## 3. glTF로 합치기

프로젝트 루트(`CGFP` 폴더, `tools`가 있는 곳)에서:

```
blender --background --factory-startup --python tools/mixamo_to_gltf.py -- FishG FishH Fred Morty
```

`anim/<캐릭터>.gltf`가 생기면 게임에서 바로 쓴다 (`Order/CustomerManager.cpp`의 모델 목록).
