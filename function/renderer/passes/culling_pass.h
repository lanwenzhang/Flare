#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/swap_chain.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../gpu_resource/buffer/geometry_buffer.h"
#include "../gpu_resource/texture/texture.h" 
#include "../../scene/camera/camera.h"

namespace flare::renderer {
	using namespace flare::vk;

	struct CullingDataCPU {
		glm::vec4 planes[6];
		uint32_t  numMeshesToCull = 0;
		uint32_t  numVisibleMeshes = 0;
		uint32_t  enableFrustumCulling = 0;
		uint32_t  enableOcclusionCulling = 0;

		float zNear = 0.1f;
		float zFar = 200.0f;
		float proj00 = 1.0f;
		float proj11 = 1.0f;
	};

	class CullingPass {
	public:
		using Ptr = std::shared_ptr<CullingPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<CullingPass>(device); }
		CullingPass(Device::Ptr& device) { mDevice = device; };
		~CullingPass() = default;

		void init(const DescriptorSetLayout::Ptr& staticSetLayout, const Texture::Ptr& hzbTex, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, const GeometryBuffer::Ptr& geoBuffer,
							const DescriptorSet::Ptr& staticSet, Camera camera, int frameIndex);
		uint32_t getVisibleCount(int frameIndex) const;

	public:
		bool mEnableFrustumCulling{ false };
		bool mEnableOcclusionCulling{ false };

	private:
		void createDescriptor(const Texture::Ptr& hzbTex, int frameCount);
		void createPipeline(const DescriptorSetLayout::Ptr& staticSetLayout);
		void getFrustumCorners(glm::mat4 viewProj, glm::vec4* points);
		void getFrustumPlanes(glm::mat4 viewProj, glm::vec4* planes);
		void update(const Buffer::Ptr& indirectBuffer, const CullingDataCPU& cullingData, int frameIndex);
		void update(const Buffer::Ptr& indirectBuffer, const glm::mat4& view, const glm::mat4& proj, uint32_t numMeshesToCull, int frameIndex);
	
	private:
		Device::Ptr mDevice{ nullptr };
		ComputePipeline::Ptr mPipeline{ nullptr };
		UniformParameter::Ptr mIndirectParam{ nullptr };
		UniformParameter::Ptr mCullingDataParam{ nullptr };
		UniformParameter::Ptr mHZBParam{ nullptr };
		DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet{ nullptr };
	};
}