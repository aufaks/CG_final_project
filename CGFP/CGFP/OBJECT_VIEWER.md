# Object Viewer

`CGFP.sln`의 `ObjectViewer` 프로젝트를 선택해 빌드하고 실행한다. 기존 `CGFP` 메인 프로젝트와 별도의 실행 파일/`main()`을 사용한다.

- 실행할 때 `obj/`와 그 하위 폴더를 재귀 검색해 `.obj` 파일을 불러온다. 원래 ZIP 안에 들어있던 모델은 `obj/imported/`에 풀어 두었다.
- OBJ의 `vt`, `usemtl`, MTL의 `map_Kd`를 읽어 PNG/JPEG/BMP diffuse 텍스처를 표시한다. 이미지 선택 라이브러리는 Windows Imaging Component(WIC)다. 빠진 MTL은 모델 옆의 MTL을 찾고, diffuse가 비어 있으면 폴더 안의 컬러 이미지를 기본 텍스처로 쓴다.
- OBJ의 위치를 원점으로 옮기고 최대 길이를 2 단위로 맞춰 카메라 앞에서 보기 쉽게 정규화한다. OBJ의 면은 삼각형으로 분할하고 노멀이 없는 면은 자동 생성한다.
- 조작: `W/A/S/D` 이동, `E/Q` 상승/하강, 마우스 시점 회전, `P` 마우스 잠금/해제 토글, `←/→` 이전/다음 모델, `Esc` 종료.
- 시점 감도는 `0.05`이며, 8번 Krusty Krab 전체 맵 모델은 사람 크기 모델 대비 96배 크기로 표시한다. 이 모델을 선택하면 카메라가 집게리아 건물 내부에서 시작하고, 맵 안을 살펴보기 좋도록 이동 속도를 낮춘다.

현재 확인된 10개 모델은 Bubble Bass, Mr Krabs, Sushi Costume 00, Fish G, Fish H, SpongeBob, Squidward's House, Krusty Krab, Fred, Morty다. 일부 패키지는 MTL이 OBJ의 `mtllib` 이름과 다르거나 MTL이 빠져 있어, 같은 폴더에서 재질 파일을 찾거나 관련 diffuse 이미지를 기본 텍스처로 연결한다.
- 셰이더는 `shaders/object_viewer.vert`, `shaders/object_viewer.frag`에서 읽는다.

추가 모델은 `CGFP/CGFP/obj/` 아래에 `.obj`로 넣으면 된다. 재질/텍스처를 사용하는 모델의 외형은 현재 뷰어에서 단색 조명으로 표시한다.
