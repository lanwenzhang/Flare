#include "static_uniform_manager.h"

namespace flare::renderer {

	StaticUniformManager::StaticUniformManager(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const Scene& scene, const MeshData& meshData, 
                                               const GeometryBuffer::Ptr& buffer, const AccelerationStructureBuffer& tlas, int frameCount){

		mDevice = device;
		mCommandPool = commandPool;

        mSphereParam = buildSphere(buffer->getSphereBuffer(), frameCount);
        mTransformParam = buildTransforms(mDevice, scene.globalTransform.size(), scene.globalTransform.data(), frameCount);
        mDrawDataParam = buildDrawData(mDevice, scene.drawDataArray.size(), scene.drawDataArray.data(), frameCount);

        SamplerDesc sAlbedo{}; 
        sAlbedo.minFilter = sAlbedo.magFilter = VK_FILTER_LINEAR;
        sAlbedo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sAlbedo.anisotropyEnable = VK_TRUE;  sAlbedo.maxAnisotropy = 16.0f;
        sAlbedo.addressU = sAlbedo.addressV = sAlbedo.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        SamplerDesc sNormal{}; 
        sNormal.minFilter = sNormal.magFilter = VK_FILTER_LINEAR;
        sNormal.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sNormal.anisotropyEnable = VK_TRUE;  sNormal.maxAnisotropy = 16.0f;
        sNormal.addressU = sNormal.addressV = sNormal.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        SamplerDesc sMR{};
        sMR.minFilter = sMR.magFilter = VK_FILTER_NEAREST;
        sMR.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sMR.anisotropyEnable = VK_FALSE;
        sMR.addressU = sMR.addressV = sMR.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        // set texture index offset and format
        std::vector<TextureFiles> textureFiles;
        auto addList = [&](const std::vector<std::string>& files, VkFormat fmt, const SamplerDesc& samp, uint32_t& baseOut) {
            baseOut = static_cast<uint32_t>(textureFiles.size());
            for (auto& f : files) textureFiles.push_back({ f, fmt, samp});
        };

        addList(meshData.baseColorTexturePath, VK_FORMAT_R8G8B8A8_SRGB, sAlbedo, mTextureOffsets.baseDiffuse);
        addList(meshData.metallicRoughnessTexturePath, VK_FORMAT_R8G8B8A8_UNORM, sMR, mTextureOffsets.baseMR);
        addList(meshData.normalTexturePath, VK_FORMAT_R8G8B8A8_UNORM, sNormal, mTextureOffsets.baseNormal);

        mMaterialParam = buildMaterials(mDevice, meshData.materials.size(), meshData.materials.data(), frameCount);
        mTextureArray = buildTextureArray(mDevice, commandPool, textureFiles, frameCount);
        mTLASParam = buildTLAS(tlas, frameCount);
	}

    UniformParameter::Ptr StaticUniformManager::buildSphere(const Buffer::Ptr& sphereBuffer, int frameCount) {
        auto p = UniformParameter::create();
        p->mBinding = BINDING_SPHERE;
        p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        p->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        p->mCount = 1;
        p->mSize = VK_WHOLE_SIZE;

        for (int i = 0; i < frameCount; ++i)
            p->mBuffers.push_back(sphereBuffer);

        return p;
    }

    UniformParameter::Ptr StaticUniformManager::buildTransforms(const Device::Ptr& device, size_t transformCount, const glm::mat4* initialData, int frameCount){
        auto p = UniformParameter::create();
        p->mBinding = BINDING_TRANSFORMS;
        p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        p->mStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        p->mCount = 1;
        p->mSize = sizeof(glm::mat4) * transformCount;

        for (int i = 0; i < frameCount; ++i) {
            auto buf = Buffer::createStorageBuffer(device, p->mSize, initialData, false);
            p->mBuffers.push_back(buf);
        }
        return p;
    }

    UniformParameter::Ptr StaticUniformManager::buildDrawData(const Device::Ptr& device, size_t drawCount, const DrawData* initialData, int frameCount)
    {
        auto p = UniformParameter::create();
        p->mBinding = BINDING_DRAWDATA;
        p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        p->mStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        p->mCount = 1;
        p->mSize = sizeof(loader::DrawData) * drawCount;

        for (int i = 0; i < frameCount; ++i) {
            auto buf = Buffer::createStorageBuffer(device, p->mSize, initialData, false);
            p->mBuffers.push_back(buf);
        }
        return p;
    }

    UniformParameter::Ptr StaticUniformManager::buildMaterials(const Device::Ptr& device, size_t materialCount, const Material* initialData, int frameCount){
  
        std::vector<GpuMaterial> gpuMaterials;
        gpuMaterials.reserve(materialCount);

        if (initialData) {
            for (size_t i = 0; i < materialCount; ++i) {
                const auto& cpuMat = initialData[i];
                GpuMaterial gm{};
                gm.emissiveFactor = cpuMat.emissiveFactor;
                gm.baseColorFactor = cpuMat.baseColorFactor;
                gm.roughnessFactor = cpuMat.roughnessFactor;
                gm.metallicFactor = cpuMat.metallicFactor;
                gm.alphaCutoff = cpuMat.alphaCutoff;
                gm.alphaMode = cpuMat.alphaMode;
                gm.baseColorTexture = cpuMat.baseColorTexture;
                gm.metallicRoughnessTexture = (cpuMat.metallicRoughnessTexture >= 0) ? (cpuMat.metallicRoughnessTexture + mTextureOffsets.baseMR): 0xFFFFFFFFu;
                gm.normalTexture = cpuMat.normalTexture + mTextureOffsets.baseNormal;
                gm.emissiveTexture = cpuMat.emissiveTexture;
                gpuMaterials.push_back(gm);
            }
        }

        auto p = UniformParameter::create();
        p->mBinding = BINDING_MATERIALS;
        p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        p->mStage = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        p->mCount = 1;
        p->mSize = sizeof(GpuMaterial) * materialCount;

        for (int i = 0; i < frameCount; ++i) {
            auto buf = Buffer::createStorageBuffer(device, p->mSize,
                gpuMaterials.empty() ? nullptr : gpuMaterials.data(), false);
            p->mBuffers.push_back(buf);
        }
        return p;
     }

    UniformParameter::Ptr StaticUniformManager::buildTextureArray(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::vector<TextureFiles>& textureFiles, int frameCount){
        auto p = UniformParameter::create();
        p->mBinding = BINDING_TEX_BINDLESS;
        p->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        p->mStage = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        p->mCount = static_cast<uint32_t>(textureFiles.size());
        p->mTextures.resize(p->mCount, nullptr);
        p->mImageInfos.resize(p->mCount * frameCount);

        for (size_t i = 0; i < textureFiles.size(); ++i) {
           auto tex = Texture::create(device, commandPool, textureFiles[i].path, textureFiles[i].format, textureFiles[i].sampler);
           p->mTextures[i] = tex;
           for (int f = 0; f < frameCount; ++f) {
               p->mImageInfos[i * frameCount + f] = tex->getImageInfo();
           }
        }
        return p;
    }

    UniformParameter::Ptr StaticUniformManager::buildTLAS(const AccelerationStructureBuffer& tlas, int frameCount){
        auto p = UniformParameter::create();
        p->mBinding = BINDING_TLAS;
        p->mDescriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        p->mStage = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;
        p->mCount = 1;
        p->mAccelerationStructure.push_back(tlas.handle);
        return p;
    }
}