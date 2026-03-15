#pragma once

#include "../../common.h"
#include "../scene/scene_graph.h"

namespace flare::loader {

    using namespace flare::scene;

    struct gltfSpan {
        const uint8_t* ptr = nullptr;
        size_t stride = 0;
        size_t count = 0;
    };

    struct BoundingBox {
        BoundingBox() = default;
        BoundingBox(const glm::vec3& mn, const glm::vec3& mx) : minPt(mn), maxPt(mx) {}

        glm::vec3 minPt = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 maxPt = glm::vec3(-std::numeric_limits<float>::max());

        inline void expand(const glm::vec3& p) {
            minPt = glm::min(minPt, p);
            maxPt = glm::max(maxPt, p);
        }

        BoundingBox getTransformed(const glm::mat4& M) const {
            BoundingBox out;
            const glm::vec3 v[8] = {
                {minPt.x, minPt.y, minPt.z}, {maxPt.x, minPt.y, minPt.z},
                {minPt.x, maxPt.y, minPt.z}, {maxPt.x, maxPt.y, minPt.z},
                {minPt.x, minPt.y, maxPt.z}, {maxPt.x, minPt.y, maxPt.z},
                {minPt.x, maxPt.y, maxPt.z}, {maxPt.x, maxPt.y, maxPt.z},
            };
            for (int i = 0; i < 8; ++i) {
                glm::vec3 p = glm::vec3(M * glm::vec4(v[i], 1.0f));
                out.expand(p);
            }
            return out;
        }
    };

    struct BoundingBoxGPU { float pt[6];};

    inline BoundingBoxGPU toGPU(const BoundingBox& b) {
        BoundingBoxGPU g{};
        g.pt[0] = b.minPt.x; g.pt[1] = b.minPt.y; g.pt[2] = b.minPt.z;
        g.pt[3] = b.maxPt.x; g.pt[4] = b.maxPt.y; g.pt[5] = b.maxPt.z;
        return g;
    }

    struct BoundingSphereGPU {
        glm::vec4 centerAndRadius;
    };

    inline float maxScale(const glm::mat4& M) {
        const float sx = glm::length(glm::vec3(M[0]));
        const float sy = glm::length(glm::vec3(M[1]));
        const float sz = glm::length(glm::vec3(M[2]));
        return std::max(sx, std::max(sy, sz));
    }

    inline BoundingSphereGPU toSphereGPU(const glm::vec3& centerLocal, float radiusLocal, const glm::mat4& world) {
        const glm::vec3 centerWorld = glm::vec3(world * glm::vec4(centerLocal, 1.0f));
        const float s = maxScale(world);
        return { glm::vec4(centerWorld, radiusLocal * s) };
    }

    struct Mesh {
        uint32_t indexOffset = 0;
        uint32_t vertexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t materialID = 0;
    };
                       
    enum class AlphaMode : uint32_t {
        Opaque = 0,
        Mask = 1,
        Blend = 2
    };

    struct Material {
        glm::vec4 baseColorFactor;
        glm::vec4 emissiveFactor;

        float metallicFactor;
        float roughnessFactor;
        float alphaCutoff;
        uint32_t alphaMode;

        uint32_t baseColorTexture;
        uint32_t metallicRoughnessTexture;
        uint32_t normalTexture;
        uint32_t emissiveTexture;
    };

    struct MeshData {
        std::vector<uint32_t> indexData;
        std::vector<uint8_t> vertexData;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<BoundingBox> boxes;
        std::vector<glm::vec4> spheres;

        std::vector<std::string> baseColorTexturePath;
        std::vector<std::string> metallicRoughnessTexturePath;
        std::vector<std::string> normalTexturePath;
    };

    bool loadgltf(const std::string& path, MeshData& meshData, flare::scene::Scene& scene);
    bool loadobj(const std::string& path, MeshData& meshData);
}

