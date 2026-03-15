#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "loader.h"

namespace flare::loader {
    // 1 material
    // 1.1 load texture
    static int addTextureIfUnique(std::vector<std::string>& textureFiles, std::unordered_map<std::string, int>& textureMap, const std::string& texPath) {
        auto it = textureMap.find(texPath);
        if (it != textureMap.end()) {
            return it->second;
        }
        else {
            int id = static_cast<int>(textureFiles.size());
            textureFiles.push_back(texPath);
            textureMap[texPath] = id;
            return id;
        }
    }

    // 1.2 build materials
    static void buildMaterials(const nlohmann::json& gltf, const std::filesystem::path& base, MeshData& meshData) {
        
        meshData.baseColorTexturePath.clear();
        meshData.metallicRoughnessTexturePath.clear();
        meshData.normalTexturePath.clear();

        std::unordered_map<std::string, int> albedoMap, MetallicRoughnessMap, normalMap;
        auto loadTexture = [&](const nlohmann::json& texInfo, uint32_t& idOut, std::vector<std::string>& pathList, 
                               std::unordered_map<std::string, int>& textureMap) {

                int texIdx = (int)texInfo["index"];
                const auto& gltfSource = gltf["textures"][texIdx];
                int imageIdx = (int)gltfSource["source"];
                const auto& image = gltf["images"][imageIdx];

                std::filesystem::path texturePath = (base / std::string(image["uri"])).lexically_normal();
                idOut = (uint32_t)addTextureIfUnique(pathList, textureMap, texturePath.string());
        };

        for (size_t i = 0; i < gltf["materials"].size(); ++i) {
            const auto& gm = gltf["materials"][i];
            Material m{};

            if (gm.contains("pbrMetallicRoughness")) {
                const auto& pbr = gm["pbrMetallicRoughness"];

                if (pbr.contains("baseColorFactor") && pbr["baseColorFactor"].is_array() && pbr["baseColorFactor"].size() == 4) {
                    const auto& v = pbr["baseColorFactor"];
                    m.baseColorFactor = glm::vec4(
                        v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>());
                }

                if (pbr.contains("roughnessFactor") && pbr["roughnessFactor"].is_number())
                    m.roughnessFactor = pbr["roughnessFactor"].get<float>();
                else m.roughnessFactor = 1.0;
                if (pbr.contains("metallicFactor") && pbr["metallicFactor"].is_number())
                    m.metallicFactor = pbr["metallicFactor"].get<float>();
                else m.roughnessFactor = 0.0;
                if (pbr.contains("baseColorTexture") && pbr["baseColorTexture"].contains("index")
                    && pbr["baseColorTexture"]["index"].is_number_integer())
                    loadTexture(pbr["baseColorTexture"], m.baseColorTexture, meshData.baseColorTexturePath, albedoMap);
                if (pbr.contains("metallicRoughnessTexture") && pbr["metallicRoughnessTexture"].contains("index")
                    && pbr["metallicRoughnessTexture"]["index"].is_number_integer())
                    loadTexture(pbr["metallicRoughnessTexture"], m.metallicRoughnessTexture, meshData.metallicRoughnessTexturePath, MetallicRoughnessMap);
                else m.metallicRoughnessTexture = -1;
            }
            if (gm.contains("normalTexture") && gm["normalTexture"].contains("index")
                && gm["normalTexture"]["index"].is_number_integer())
                loadTexture(gm["normalTexture"], m.normalTexture, meshData.normalTexturePath, normalMap);

            if (gm.contains("alphaMode") && gm["alphaMode"].is_string()) {
                const std::string mode = gm["alphaMode"];
                if (mode == "MASK")  m.alphaMode = 1u;
                else if (mode == "BLEND") m.alphaMode = 2u;
                else                      m.alphaMode = 0u;
            }
            else {
                m.alphaMode = 0u;
            }

            if (gm.contains("alphaCutoff") && gm["alphaCutoff"].is_number())
                m.alphaCutoff = gm["alphaCutoff"].get<float>();

            meshData.materials.push_back(m);
        }
    }

    // 2 mesh
    // 2.1 primitives
    // 2.1.1 load buffers
    static bool readBin(const std::filesystem::path& path, std::vector<uint8_t>& out) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios::end);
        std::streamsize s = file.tellg();
        out.resize(static_cast<size_t>(s));

        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(out.data()), s);
        return true;
    }

    static size_t gltfTypeComposition(const std::string& type) {
        if (type == "SCALAR") return 1; if (type == "VEC2") return 2; if (type == "VEC3") return 3;
        if (type == "VEC4") return 4; if (type == "MAT2") return 4; if (type == "MAT3") return 9; if (type == "MAT4") return 16;
        return 0;
    }

    static size_t gltfComponentSize(int componentType) {
        if (componentType == 5126) return 4; if (componentType == 5125) return 4; if (componentType == 5123) return 2; return 0;
    }

    static bool loadBuffers(const nlohmann::json& gltf, const std::filesystem::path& base, std::vector<std::vector<uint8_t>>& buffers) {
        for (const auto& b : gltf["buffers"]) {
            if (!b.contains("uri")) return false;
            std::filesystem::path binPath = (base / std::string(b["uri"])).lexically_normal();
            std::vector<uint8_t> bin;
            if (!readBin(binPath, bin)) return false;
            buffers.emplace_back(std::move(bin));
        }
        return true;
    }

    static gltfSpan makeSpan(const nlohmann::json& gltf, size_t accessorIndex, const std::vector<std::vector<uint8_t>>& buffers) {

        const auto& accessor = gltf["accessors"].at(accessorIndex);
        const auto& bufferView = gltf["bufferViews"].at((size_t)accessor.value("bufferView", 0));
        const size_t accessorOffset = (size_t)accessor.value("byteOffset", 0);
        const size_t bufferViewOffset = (size_t)bufferView.value("byteOffset", 0);
        const size_t bufferViewStride = (size_t)bufferView.value("byteStride", 0);
        const size_t count = (size_t)accessor.at("count");

        const size_t compCount = gltfTypeComposition(accessor.at("type"));
        const size_t compSize = gltfComponentSize(accessor.at("componentType"));
        const size_t elementSize = compCount * compSize;
        const size_t stride = bufferViewStride ? bufferViewStride : elementSize;

        const size_t bufferIndex = (size_t)bufferView.at("buffer");
        const auto& bin = buffers.at(bufferIndex);
        const size_t base = accessorOffset + bufferViewOffset;
        return gltfSpan{ bin.data() + base, stride, count };
    }

    // 2.1.2 load primitives
    static void buildVertex(float* vf, const float* p, const float* uv, const float* n, const float* t) {
        vf[0] = p ? p[0] : 0.0f; vf[1] = p ? p[1] : 0.0f; vf[2] = p ? p[2] : 0.0f;
        vf[3] = uv ? uv[0] : 0.0f; vf[4] = uv ? uv[1] : 0.0f;
        vf[5] = n ? n[0] : 0.0f; vf[6] = n ? n[1] : 0.0f; vf[7] = n ? n[2] : 1.0f;
        vf[8] = t ? t[0] : 1.0f; vf[9] = t ? t[1] : 0.0f; vf[10] = t ? t[2] : 0.0f; vf[11] = t ? t[3] : 1.0f;
    }

    static void loadPrimitives(const nlohmann::json& gltf, const  nlohmann::json& prim, const std::vector<std::vector<uint8_t>>& buffers, MeshData& meshData, uint32_t materialId) {
        // vertex
        const uint32_t strideFloats = 12;
        const uint32_t strideBytes = strideFloats * sizeof(float);

        const auto& at = prim["attributes"];
        size_t accPos = (size_t)at["POSITION"];
        size_t accUV = at.contains("TEXCOORD_0") ? (size_t)at["TEXCOORD_0"] : SIZE_MAX;
        size_t accNor = at.contains("NORMAL") ? (size_t)at["NORMAL"] : SIZE_MAX;
        size_t accTan = at.contains("TANGENT") ? (size_t)at["TANGENT"] : SIZE_MAX;

        gltfSpan sPos = makeSpan(gltf, accPos, buffers);
        gltfSpan sNor = (accNor != SIZE_MAX) ? makeSpan(gltf, accNor, buffers) : gltfSpan{};
        gltfSpan sUV = (accUV != SIZE_MAX) ? makeSpan(gltf, accUV, buffers) : gltfSpan{};
        gltfSpan sTan = (accTan != SIZE_MAX) ? makeSpan(gltf, accTan, buffers) : gltfSpan{};

        const size_t vCount = sPos.count;
        const uint32_t vertexBase = (uint32_t)(meshData.vertexData.size() / strideBytes);
        meshData.vertexData.resize(meshData.vertexData.size() + vCount * strideBytes);

        for (size_t i = 0; i < vCount; ++i) {
            float* vf = reinterpret_cast<float*>(meshData.vertexData.data() + (vertexBase + (uint32_t)i) * strideBytes);
            const float* p = reinterpret_cast<const float*>(sPos.ptr + i * sPos.stride);
            const float* uv = sUV.ptr ? reinterpret_cast<const float*>(sUV.ptr + std::min(i, sUV.count - 1) * sUV.stride) : nullptr;
            const float* n = sNor.ptr ? reinterpret_cast<const float*>(sNor.ptr + std::min(i, sNor.count - 1) * sNor.stride) : nullptr;
            const float* t = sTan.ptr ? reinterpret_cast<const float*>(sTan.ptr + std::min(i, sTan.count - 1) * sTan.stride) : nullptr;
            buildVertex(vf, p, uv, n, t);
        }

        // index
        std::vector<uint32_t> localIdx;
        const size_t accIdx = (size_t)prim["indices"];
        gltfSpan sIdx = makeSpan(gltf, accIdx, buffers);
        const auto& acc = gltf["accessors"][accIdx];
        const int comp = (int)acc["componentType"];
        localIdx.resize(sIdx.count);

        for (size_t i = 0; i < sIdx.count; ++i) {
            uint32_t v = 0;
            if (comp == 5125)      v = *reinterpret_cast<const uint32_t*>(sIdx.ptr + i * sIdx.stride);
            else if (comp == 5123) v = *reinterpret_cast<const uint16_t*>(sIdx.ptr + i * sIdx.stride);
            else if (comp == 5121) v = *reinterpret_cast<const uint8_t*>(sIdx.ptr + i * sIdx.stride);
            else { return; } 
            localIdx[i] = v;
        }

        // mesh
        Mesh m{};
        m.indexOffset = (uint32_t)meshData.indexData.size();
        m.indexCount = (uint32_t)localIdx.size();
        m.vertexOffset = (int32_t)vertexBase;
        m.materialID = materialId;

        meshData.indexData.insert(meshData.indexData.end(), localIdx.begin(), localIdx.end());
        meshData.meshes.push_back(m);
    }

    // 2.2 build meshes
    static void buildMeshes(const nlohmann::json& gltf, const std::vector<std::vector<uint8_t>>& buffers, MeshData& meshData) {

        meshData.meshes.clear();
        meshData.indexData.clear();
        meshData.vertexData.clear();

        for (size_t mi = 0; mi < gltf["meshes"].size(); ++mi) {
            const auto& gmesh = gltf["meshes"][mi];
            for (const auto& prim : gmesh["primitives"]) {
                uint32_t matId = 0;
                if (prim.contains("material")) {
                    int mid = (int)prim["material"];
                    if (mid >= 0 && mid < (int)meshData.materials.size()) matId = (uint32_t)mid;
                }
                loadPrimitives(gltf, prim, buffers, meshData, matId);
            }
        }
    }

    // 3 scene
    // 3.1 transform
    static glm::mat4 loadTransform(const nlohmann::json& node) {
        glm::vec3 S(1.0f);
        if (auto it = node.find("scale"); it != node.end() && it->is_array() && it->size() == 3) {
            S = { (*it)[0], (*it)[1], (*it)[2] };
        }
        return glm::scale(glm::mat4(1.0f), S);
    }

    // 3.2 build scene
    static void buildScene(const nlohmann::json& gltf, const flare::loader::MeshData& meshData, flare::scene::Scene& scene) {
        
        auto addNode = [&](int parent, int level)->int {
            const int id = (int)scene.hierarchy.size();
            scene.hierarchy.push_back({});
            scene.localTransform.push_back(glm::mat4(1.0f));
            scene.globalTransform.push_back(glm::mat4(1.0f));
            auto& h = scene.hierarchy[id];
            h.parent = parent; h.level = level;
            if (parent >= 0) {
                auto& ph = scene.hierarchy[parent];
                if (ph.firstChild == -1) ph.firstChild = id;
                if (ph.lastSibling != -1) scene.hierarchy[ph.lastSibling].nextSibling = id;
                ph.lastSibling = id;
            }
            return id;
        };

        // root node
        const int root = addNode(-1, 0);
        int nodeIdx = -1;
        if (auto itS = gltf.find("scene"); itS != gltf.end() && gltf.contains("scenes")) {
            int s = (int)*itS;
            const auto& sc = gltf["scenes"][s];
            if (auto ns = sc.find("nodes"); ns != sc.end() && ns->is_array() && !ns->empty())
                nodeIdx = (int)(*ns)[0];
        }

        // subnode transform
        int n = addNode(root, 1);
        scene.localTransform[n] = (nodeIdx >= 0) ? loadTransform(gltf["nodes"][nodeIdx]) : glm::mat4(1.0f);
        scene.globalTransform[n] = scene.localTransform[n];

        // draw data
        scene.drawDataArray.reserve(meshData.meshes.size());
        for (uint32_t m = 0; m < (uint32_t)meshData.meshes.size(); ++m) {
            const uint32_t matId = meshData.meshes[m].materialID;
            scene.drawDataArray.push_back({ (uint32_t)n, matId });
        }
    }

    // 4 bound
    // 4.1 aabb
    void static buildAABB(MeshData& meshData) {

        const uint32_t strideFloats = 12u;
        const uint32_t strideBytes = strideFloats * sizeof(float);

        meshData.boxes.clear();
        meshData.boxes.reserve(meshData.meshes.size());

        for (const Mesh& mesh : meshData.meshes) {
            const uint32_t numIndices = mesh.indexCount;

            glm::vec3 vmin(std::numeric_limits<float>::max());
            glm::vec3 vmax(std::numeric_limits<float>::lowest());

            for (uint32_t i = 0; i < numIndices; ++i) {
                const uint32_t idxLocal = meshData.indexData[mesh.indexOffset + i];
                const uint32_t vtxIndex = idxLocal + mesh.vertexOffset;

                const float* vf = reinterpret_cast<const float*>(meshData.vertexData.data() + static_cast<size_t>(vtxIndex) * strideBytes);

                const glm::vec3 p(vf[0], vf[1], vf[2]);
                vmin = glm::min(vmin, p);
                vmax = glm::max(vmax, p);
            }
            meshData.boxes.emplace_back(vmin, vmax);
        }
    }
    // 4.2 sphere
    void static buildSpheres(MeshData& meshData) {
          
        const uint32_t strideFloats = 12u;
        const uint32_t strideBytes = strideFloats * sizeof(float);

        meshData.spheres.clear();
        meshData.spheres.reserve(meshData.meshes.size());

        for (const BoundingBox& box : meshData.boxes)
        {
            glm::vec3 center = 0.5f * (box.minPt + box.maxPt);
            float radius = 0.5f * glm::length(box.maxPt - box.minPt);
            meshData.spheres.emplace_back(center, radius);
        }
    }

    // 5 final load function
    bool loadgltf(const std::string& path, MeshData& meshData, flare::scene::Scene& scene) {

        std::ifstream jf(path);
        if (!jf) { std::cerr << "Failed to open gltf: " << path << "\n"; return false; }
        nlohmann::json gltf; jf >> gltf;

        std::filesystem::path base = std::filesystem::path(path).parent_path();
        std::vector<std::vector<uint8_t>> buffers;
        if (!loadBuffers(gltf, base, buffers)) { std::cerr << "Failed to load buffers\n"; return false; }

        buildMaterials(gltf, base, meshData);
        buildMeshes(gltf, buffers, meshData);
        buildScene(gltf, meshData, scene);
        buildAABB(meshData);
        buildSpheres(meshData);
        return true;
    }

    bool loadobj(const std::string& path, MeshData& meshData){
   
        meshData.vertexData.clear();
        meshData.indexData.clear();
        meshData.meshes.clear();

        Assimp::Importer importer;

        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |          
            aiProcess_CalcTangentSpace;

        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
            return false;
        }

        const uint32_t floatStride = 11u;
        uint32_t baseVertex = 0;
        uint32_t baseIndex = 0;

        for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
            const aiMesh* aiMesh = scene->mMeshes[mi];
            if (!aiMesh || aiMesh->mNumVertices == 0) {
                continue;
            }
            for (unsigned int v = 0; v < aiMesh->mNumVertices; ++v) {
                glm::vec3 pos(0.0f);
                if (aiMesh->HasPositions()) {
                    pos.x = aiMesh->mVertices[v].x;
                    pos.y = aiMesh->mVertices[v].y;
                    pos.z = aiMesh->mVertices[v].z;
                }
                float vertex[3] = { pos.x, pos.y, pos.z };

                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(vertex);
                meshData.vertexData.insert(meshData.vertexData.end(), ptr, ptr + sizeof(vertex));
            }
            for (unsigned int f = 0; f < aiMesh->mNumFaces; ++f) {
                const aiFace& face = aiMesh->mFaces[f];
                if (face.mNumIndices != 3) {
                    continue;
                }
                meshData.indexData.push_back(baseVertex + static_cast<uint32_t>(face.mIndices[0]));
                meshData.indexData.push_back(baseVertex + static_cast<uint32_t>(face.mIndices[1]));
                meshData.indexData.push_back(baseVertex + static_cast<uint32_t>(face.mIndices[2]));
            }

            baseVertex += static_cast<uint32_t>(aiMesh->mNumVertices);
        }

        Mesh out{};
        out.indexCount = static_cast<uint32_t>(meshData.indexData.size());
        out.vertexOffset = 0;

        meshData.meshes.push_back(out);
        return !meshData.vertexData.empty() && !meshData.indexData.empty();
    }
}




