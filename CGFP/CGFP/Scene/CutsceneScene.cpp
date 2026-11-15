#include "Scene/CutsceneScene.h"

#include "Core/InputManager.h"
#include "Render/ResourceManager.h"
#include "Scene/PlayScene.h"
#include "Scene/SceneManager.h"

#include <iostream>
#include <memory>

void CutsceneScene::Enter() {
    InputManager::SetCursorCaptured(false);

    // TODO(4단계): 기획서 대사·연출로 교체
    cutscene.AddLine("집게사장", "자네가 오늘부터 일할 신입인가?");
    cutscene.AddLine("집게사장", "손님이 오면 주문을 받고, 게살 버거를 만들어 자리로 가져다주게.");
    cutscene.AddLine("집게사장", "하루 할당량을 못 채우면... 알지?");
    cutscene.AddCameraKey({ 0.0f, glm::vec3(0.0f, 1.6f, 4.0f), -90.0f, 0.0f });
    cutscene.AddCameraKey({ 4.0f, glm::vec3(0.0f, 1.4f, 2.0f), -90.0f, -5.0f });
    cutscene.Start();
    ShowCurrentLine();
}

void CutsceneScene::Update(float deltaTime) {
    cutscene.Update(deltaTime);
    if (InputManager::GetKeyDown(GLFW_KEY_ENTER)) {
        cutscene.NextLine();
        ShowCurrentLine();
    }
    if (InputManager::GetKeyDown(GLFW_KEY_SPACE)) cutscene.Skip();
    if (cutscene.IsFinished()) sceneManager->ChangeScene(std::make_unique<PlayScene>());
}

void CutsceneScene::ShowCurrentLine() {
    // TODO(4단계): 대사창 UI. 지금은 창 제목과 콘솔에 표시
    const DialogueLine* line = cutscene.GetCurrentLine();
    if (!line) return;
    const std::string text = line->speaker + ": " + line->text + "  (Enter: 다음, Space: 건너뛰기)";
    glfwSetWindowTitle(InputManager::GetWindow(), text.c_str());
    std::cout << text << "\n";
}

void CutsceneScene::Render() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(InputManager::GetWindow(), &width, &height);
    if (width == 0 || height == 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.2f, 0.35f, 0.55f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cutscene.ApplyCamera(camera);
    ShaderManager& shaders = ResourceManager::Get().Shaders();
    shaders.Use("basic");
    shaders.SetMat4("uView", camera.GetViewMatrix());
    shaders.SetMat4("uProjection", camera.GetProjectionMatrix(static_cast<float>(width) / height));
    shaders.SetVec3("uLightDirection", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    shaders.SetInt("uTexture", 0);

    // 집게사장 자리표시(빨간 상자). TODO(4단계): 집게사장 OBJ로 교체
    const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.8f, 0.0f)), glm::vec3(0.9f, 1.6f, 0.6f));
    shaders.SetMat4("uModel", model);
    shaders.SetVec3("uColor", glm::vec3(0.85f, 0.15f, 0.1f));
    shaders.SetBool("uUseTexture", false);
    ResourceManager::Get().GetCube()->Draw();
}
