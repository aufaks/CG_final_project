// cgltf 구현부가 fopen·strncpy를 쓴다 (MSVC 보안 경고 C4996가 오류로 설정돼 있다)
#define _CRT_SECURE_NO_WARNINGS
#include "Render/SkinnedModel.h"

#include "Render/ResourceManager.h"
#include "Render/ShaderManager.h"
#include "Render/Texture.h"

#define CGLTF_IMPLEMENTATION
#include "ThirdParty/cgltf.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>

namespace {
struct SkinnedVertex {
    glm::vec3 position{ 0.0f };
    glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
    glm::vec2 uv{ 0.0f };
    uint16_t joints[4]{};
    glm::vec4 weights{ 0.0f };
};

std::string Directory(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

glm::mat4 ToMatrix(const NodePose& pose) {
    return glm::translate(glm::mat4(1.0f), pose.translation) * glm::mat4_cast(pose.rotation) *
           glm::scale(glm::mat4(1.0f), pose.scale);
}

// 키프레임 사이를 찾는다: times[i] <= time < times[i + 1]
void FindKey(const std::vector<float>& times, float time, size_t& index, float& t) {
    if (times.size() < 2 || time <= times.front()) {
        index = 0;
        t = 0.0f;
        return;
    }
    if (time >= times.back()) {
        index = times.size() - 2;
        t = 1.0f;
        return;
    }
    const auto it = std::upper_bound(times.begin(), times.end(), time);
    index = static_cast<size_t>(it - times.begin()) - 1;
    const float span = times[index + 1] - times[index];
    t = span > 0.0f ? (time - times[index]) / span : 0.0f;
}
} // namespace

SkinnedModel::~SkinnedModel() {
    Release();
}

void SkinnedModel::Release() {
    for (Primitive& p : primitives) {
        if (p.ebo) glDeleteBuffers(1, &p.ebo);
        if (p.vbo) glDeleteBuffers(1, &p.vbo);
        if (p.vao) glDeleteVertexArrays(1, &p.vao);
    }
    primitives.clear();
}

bool SkinnedModel::Load(const std::string& gltfPath) {
    Release();
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, gltfPath.c_str(), &data) != cgltf_result_success) {
        std::cerr << "[SkinnedModel] glTF를 열 수 없음: " << gltfPath << "\n";
        return false;
    }
    if (cgltf_load_buffers(&options, data, gltfPath.c_str()) != cgltf_result_success) {
        std::cerr << "[SkinnedModel] glTF 버퍼(.bin)를 읽을 수 없음: " << gltfPath << "\n";
        cgltf_free(data);
        return false;
    }
    const std::string folder = Directory(gltfPath);
    auto nodeIndex = [&](const cgltf_node* node) { return node ? static_cast<int>(node - data->nodes) : -1; };

    // 노드 계층과 기본 자세
    const size_t nodeCount = data->nodes_count;
    nodeNames.assign(nodeCount, std::string());
    parents.assign(nodeCount, -1);
    restPose.assign(nodeCount, NodePose{});
    for (size_t i = 0; i < nodeCount; ++i) {
        const cgltf_node& node = data->nodes[i];
        nodeNames[i] = node.name ? node.name : "";
        parents[i] = nodeIndex(node.parent);
        NodePose& pose = restPose[i];
        if (node.has_matrix) { // 행렬로 저장된 노드는 드물다 (Blender는 TRS로 내보낸다). 이동만 살린다.
            pose.translation = glm::vec3(node.matrix[12], node.matrix[13], node.matrix[14]);
        }
        if (node.has_translation) pose.translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        if (node.has_rotation) pose.rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        if (node.has_scale) pose.scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
    }
    order.clear();
    std::vector<bool> visited(nodeCount, false);
    std::function<void(int)> visit = [&](int i) {
        if (visited[i]) return;
        if (parents[i] >= 0) visit(parents[i]);
        visited[i] = true;
        order.push_back(i);
    };
    for (size_t i = 0; i < nodeCount; ++i) visit(static_cast<int>(i));

    // 스킨 (첫 번째 것만 쓴다)
    joints.clear();
    inverseBind.clear();
    if (data->skins_count > 0) {
        const cgltf_skin& skin = data->skins[0];
        for (size_t j = 0; j < skin.joints_count; ++j) {
            joints.push_back(nodeIndex(skin.joints[j]));
            glm::mat4 m(1.0f);
            if (skin.inverse_bind_matrices) cgltf_accessor_read_float(skin.inverse_bind_matrices, j, glm::value_ptr(m), 16);
            inverseBind.push_back(m);
        }
    }
    if (joints.empty() || joints.size() > static_cast<size_t>(kMaxBones)) {
        std::cerr << "[SkinnedModel] 뼈 개수가 0이거나 " << kMaxBones << "개를 넘음(" << joints.size()
                  << "): " << gltfPath << " — Mixamo에서 손가락 없는 뼈대로 받으세요.\n";
        cgltf_free(data);
        return false;
    }

    // 메시: 스킨이 붙은 노드의 메시만 그린다
    cpuPositions.clear();
    cpuJoints.clear();
    cpuWeights.clear();
    boundsMin = glm::vec3(FLT_MAX);
    boundsMax = glm::vec3(-FLT_MAX);
    for (size_t n = 0; n < nodeCount; ++n) {
        const cgltf_node& node = data->nodes[n];
        if (!node.mesh || !node.skin) continue;
        for (size_t p = 0; p < node.mesh->primitives_count; ++p) {
            const cgltf_primitive& prim = node.mesh->primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) continue;
            const cgltf_accessor *position = nullptr, *normal = nullptr, *uv = nullptr, *joint = nullptr, *weight = nullptr;
            for (size_t a = 0; a < prim.attributes_count; ++a) {
                const cgltf_attribute& attr = prim.attributes[a];
                if (attr.type == cgltf_attribute_type_position) position = attr.data;
                else if (attr.type == cgltf_attribute_type_normal) normal = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uv = attr.data;
                else if (attr.type == cgltf_attribute_type_joints && attr.index == 0) joint = attr.data;
                else if (attr.type == cgltf_attribute_type_weights && attr.index == 0) weight = attr.data;
            }
            if (!position || !joint || !weight) continue;

            std::vector<SkinnedVertex> vertices(position->count);
            for (size_t v = 0; v < vertices.size(); ++v) {
                SkinnedVertex& out = vertices[v];
                cgltf_accessor_read_float(position, v, glm::value_ptr(out.position), 3);
                if (normal) cgltf_accessor_read_float(normal, v, glm::value_ptr(out.normal), 3);
                if (uv) {
                    cgltf_accessor_read_float(uv, v, glm::value_ptr(out.uv), 2);
                    // glTF UV는 이미지 위쪽이 v=0. Texture는 OBJ에 맞춰 위아래를 뒤집어 올리므로 v를 뒤집는다.
                    out.uv.y = 1.0f - out.uv.y;
                }
                cgltf_uint j[4]{};
                cgltf_accessor_read_uint(joint, v, j, 4);
                for (int k = 0; k < 4; ++k) out.joints[k] = static_cast<uint16_t>(j[k]);
                cgltf_accessor_read_float(weight, v, glm::value_ptr(out.weights), 4);
                const float sum = out.weights.x + out.weights.y + out.weights.z + out.weights.w;
                if (sum > 0.0f) out.weights /= sum;
                boundsMin = glm::min(boundsMin, out.position);
                boundsMax = glm::max(boundsMax, out.position);
                cpuPositions.push_back(out.position);
                cpuJoints.emplace_back(out.joints[0], out.joints[1], out.joints[2], out.joints[3]);
                cpuWeights.push_back(out.weights);
            }
            std::vector<uint32_t> indices;
            if (prim.indices) {
                indices.resize(prim.indices->count);
                for (size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
            } else {
                indices.resize(vertices.size());
                for (size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<uint32_t>(i);
            }

            Primitive out;
            out.indexCount = static_cast<GLsizei>(indices.size());
            if (prim.material && prim.material->has_pbr_metallic_roughness) {
                const cgltf_pbr_metallic_roughness& pbr = prim.material->pbr_metallic_roughness;
                out.color = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2], pbr.base_color_factor[3]);
                const cgltf_texture* texture = pbr.base_color_texture.texture;
                if (texture && texture->image && texture->image->uri) {
                    std::string uri = texture->image->uri;
                    cgltf_decode_uri(&uri[0]);
                    uri.resize(std::strlen(uri.c_str()));
                    out.texture = ResourceManager::Get().GetTexture(folder + uri);
                }
            }

            glGenVertexArrays(1, &out.vao);
            glGenBuffers(1, &out.vbo);
            glGenBuffers(1, &out.ebo);
            glBindVertexArray(out.vao);
            glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinnedVertex), vertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
            const GLsizei stride = sizeof(SkinnedVertex);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, position)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, uv)));
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, joints)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, weights)));
            glBindVertexArray(0);
            primitives.push_back(out);
        }
    }

    // 애니메이션 클립
    clips.clear();
    for (size_t a = 0; a < data->animations_count; ++a) {
        const cgltf_animation& anim = data->animations[a];
        Clip clip;
        clip.name = anim.name ? anim.name : ("Clip" + std::to_string(a));
        for (size_t c = 0; c < anim.channels_count; ++c) {
            const cgltf_animation_channel& ch = anim.channels[c];
            if (!ch.target_node || !ch.sampler) continue;
            Channel channel;
            channel.node = nodeIndex(ch.target_node);
            if (ch.target_path == cgltf_animation_path_type_translation) channel.path = Channel::Path::Translation;
            else if (ch.target_path == cgltf_animation_path_type_rotation) channel.path = Channel::Path::Rotation;
            else if (ch.target_path == cgltf_animation_path_type_scale) channel.path = Channel::Path::Scale;
            else continue;
            const cgltf_animation_sampler& sampler = *ch.sampler;
            channel.step = sampler.interpolation == cgltf_interpolation_type_step;
            const bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;
            const size_t keys = sampler.input->count;
            channel.times.resize(keys);
            channel.values.resize(keys, glm::vec4(0.0f));
            const int components = channel.path == Channel::Path::Rotation ? 4 : 3;
            for (size_t k = 0; k < keys; ++k) {
                cgltf_accessor_read_float(sampler.input, k, &channel.times[k], 1);
                // 큐빅 스플라인은 (입력 접선, 값, 출력 접선) 순서라 가운데 값만 쓴다
                cgltf_accessor_read_float(sampler.output, cubic ? k * 3 + 1 : k, glm::value_ptr(channel.values[k]), components);
            }
            if (!channel.times.empty()) clip.duration = std::max(clip.duration, channel.times.back());
            clip.channels.push_back(std::move(channel));
        }
        clips.push_back(std::move(clip));
    }

    cgltf_free(data);
    if (primitives.empty()) {
        std::cerr << "[SkinnedModel] 스킨이 붙은 메시가 없음: " << gltfPath << "\n";
        return false;
    }
    return true;
}

const SkinnedModel::Clip* SkinnedModel::FindClip(const std::string& name) const {
    for (const Clip& clip : clips) {
        if (clip.name == name) return &clip;
    }
    return nullptr;
}

int SkinnedModel::FindNode(const std::string& name) const {
    for (size_t i = 0; i < nodeNames.size(); ++i) {
        const std::string& n = nodeNames[i];
        if (n.size() >= name.size() && n.compare(n.size() - name.size(), name.size(), name) == 0) return static_cast<int>(i);
    }
    return -1;
}

void SkinnedModel::SampleClip(const Clip& clip, float time, std::vector<NodePose>& pose) const {
    for (const Channel& ch : clip.channels) {
        if (ch.node < 0 || ch.node >= static_cast<int>(pose.size()) || ch.times.empty()) continue;
        size_t i = 0;
        float t = 0.0f;
        FindKey(ch.times, time, i, t);
        if (ch.step || ch.times.size() < 2) t = 0.0f;
        const glm::vec4& a = ch.values[i];
        const glm::vec4& b = ch.values[std::min(i + 1, ch.values.size() - 1)];
        NodePose& p = pose[ch.node];
        switch (ch.path) {
        case Channel::Path::Translation: p.translation = glm::mix(glm::vec3(a), glm::vec3(b), t); break;
        case Channel::Path::Scale: p.scale = glm::mix(glm::vec3(a), glm::vec3(b), t); break;
        case Channel::Path::Rotation:
            p.rotation = glm::slerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), t);
            break;
        }
    }
}

void SkinnedModel::ComputeGlobalMatrices(const std::vector<NodePose>& pose, std::vector<glm::mat4>& globals) const {
    globals.resize(pose.size());
    for (int i : order) {
        const glm::mat4 local = ToMatrix(pose[i]);
        globals[i] = parents[i] >= 0 ? globals[parents[i]] * local : local;
    }
}

void SkinnedModel::ComputeBoneMatrices(const std::vector<glm::mat4>& globals, std::vector<glm::mat4>& bones) const {
    bones.resize(joints.size());
    for (size_t j = 0; j < joints.size(); ++j) bones[j] = globals[joints[j]] * inverseBind[j];
}

void SkinnedModel::SkinPositions(const std::vector<glm::mat4>& bones, std::vector<glm::vec3>& out) const {
    out.resize(cpuPositions.size());
    for (size_t i = 0; i < cpuPositions.size(); ++i) {
        const glm::vec4 p(cpuPositions[i], 1.0f);
        glm::vec4 sum(0.0f);
        for (int k = 0; k < 4; ++k) {
            const unsigned j = cpuJoints[i][k];
            if (cpuWeights[i][k] > 0.0f && j < bones.size()) sum += cpuWeights[i][k] * (bones[j] * p);
        }
        out[i] = glm::vec3(sum);
    }
}

void SkinnedModel::Draw(ShaderManager& shaders, const glm::mat4& model, const std::vector<glm::mat4>& bones) const {
    const GLint bonesLocation = glGetUniformLocation(shaders.GetCurrent(), "uBones");
    if (bonesLocation >= 0 && !bones.empty()) {
        glUniformMatrix4fv(bonesLocation, static_cast<GLsizei>(std::min<size_t>(bones.size(), kMaxBones)), GL_FALSE,
                           glm::value_ptr(bones[0]));
    }
    shaders.SetMat4("uModel", model);
    shaders.SetFloat("uOpacity", 1.0f);
    for (const Primitive& p : primitives) {
        shaders.SetVec3("uColor", glm::vec3(p.color));
        const bool useTexture = p.texture && p.texture->IsLoaded();
        shaders.SetBool("uUseTexture", useTexture);
        if (useTexture) p.texture->Bind(0);
        glBindVertexArray(p.vao);
        glDrawElements(GL_TRIANGLES, p.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}
