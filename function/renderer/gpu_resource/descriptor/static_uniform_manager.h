#pragma once

#include "../../../../common.h"
#include "../../../scene/scene_graph.h"
#include "../../../loader/loader.h"
#include "../buffer/geometry_buffer.h"
#include "../texture/texture.h"
#include "../../../../platform/graphics/device.h"
#include "../../../../platform/graphics/command_pool.h"
#include "../../../../platform/graphics/buffer.h"
#include "../../../../platform/graphics/description.h"
#include "../../../../platform/graphics/acceleration_structure.h"

namespace flare::renderer {

	using namespace flare::vk;
	using namespace flare::scene;
	using namespace flare::loader;

	class StaticUniformManager {
	public:
		using Ptr = std::shared_ptr<StaticUniformManager>;
		static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const Scene& scene, const MeshData& meshData, 
						 const GeometryBuffer::Ptr& buffer, const AccelerationStructureBuffer& tlas, int frameCount)
		{ return std::make_shared<StaticUniformManager>(device, commandPool, scene, meshData, buffer, tlas, frameCount); }

		StaticUniformManager(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const Scene& scene, const MeshData& meshData, 
							 const GeometryBuffer::Ptr& buffer, const AccelerationStructureBuffer& tlas, int frameCount);
		~StaticUniformManager() = default;
		auto getParams() { return std::vector<UniformParameter::Ptr>{ mSphereParam, mTransformParam, mDrawDataParam, mMaterialParam, mTLASParam, mTextureArray}; }

	private:
		enum Binding {
			BINDING_SPHERE = 0,
			BINDING_TRANSFORMS = 1,
			BINDING_DRAWDATA = 2,
			BINDING_MATERIALS = 3,
			BINDING_TLAS = 4,
			BINDING_TEX_BINDLESS = 5,
		};

		struct GpuMaterial {
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

		struct TextureOffsets { 
			uint32_t baseDiffuse = 0; 
			uint32_t baseMR = 0; 
			uint32_t baseNormal = 0; 
		};

		struct TextureFiles { 
			std::string path; 
			VkFormat format;
			SamplerDesc sampler;
		};

		UniformParameter::Ptr buildSphere(const Buffer::Ptr& sphereBuffer, int frameCount);
		UniformParameter::Ptr buildTransforms(const Device::Ptr& device, size_t transformCount, const glm::mat4* initialData, int frameCount);
		UniformParameter::Ptr buildDrawData(const Device::Ptr& device, size_t drawCount, const DrawData* initialData, int frameCount);
		UniformParameter::Ptr buildMaterials(const Device::Ptr& device, size_t materialCount, const Material* initialData, int frameCount);
		UniformParameter::Ptr buildTextureArray(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::vector<TextureFiles>& textureFiles, int frameCount);
		UniformParameter::Ptr buildTLAS(const AccelerationStructureBuffer& tlas, int frameCount);

	private:

		Device::Ptr mDevice{ nullptr };
		CommandPool::Ptr mCommandPool{ nullptr };

		UniformParameter::Ptr mSphereParam;
		UniformParameter::Ptr mTransformParam;  
		UniformParameter::Ptr mDrawDataParam;   
		UniformParameter::Ptr mMaterialParam; 
		UniformParameter::Ptr mTextureArray;
		UniformParameter::Ptr mTLASParam;

		TextureOffsets mTextureOffsets;
	};
}


