#pragma once
#include "../../../common.h"
#include "../../../application/type.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_pool.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../../scene/camera/camera.h"
#include "../gpu_resource/texture/texture.h"
#include "geometry_pass.h"

namespace flare::renderer {
	using namespace flare::vk;

	struct GTAOParams{
		glm::vec2 resoluton;
		glm::vec2 depthUnpack;
		glm::vec2 ndcToViewMul;
		glm::vec2 ndcToViewAdd;
		glm::vec2 ndcToViewMul_x_PixelSize;

		float effectRadius{ 1.0f };
		float effectFalloffRange{ 0.5f };
		float radiusMultiplier{ 1.0f };
		float sampleDistributionPower{ 2.0f };

		float thinOccluderCompensation{ 0.0f };
		float depthMipSamplingOffset{ 0.0f };
		float finalValuePower{ 1.0f };
		float denoiseBlurBeta{ 0.4f };

		uint32_t  frameIndex{ 0 };
		uint32_t  sliceCount{ 4 };
		uint32_t  stepsPerSlice{ 3 };
		uint32_t  depthMipLevels{ 5 };

		glm::mat4 worldToView;
	};

	class GTAOPass {
	public:
		using Ptr = std::shared_ptr<GTAOPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<GTAOPass>(device); }
		GTAOPass(Device::Ptr& device) { mDevice = device; }
		~GTAOPass();
		void init(const CommandPool::Ptr& commandPool, const GeometryPass::Ptr& geometryPass, 
				  const DescriptorSetLayout::Ptr& frameSetLayout, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, Camera camera, int frameIndex);

		[[nodiscard]] auto getFilteredAO() const {
			int last = (mDenoisePasses - 1) % 2;
			return (last == 0) ? mAO[0] : mAO[1];
		}
		[[nodiscard]] auto getAO() const { return mAORaw; }

	private:
		void createImages(const CommandPool::Ptr& commandPool);
		void createPrefilerDepthDS();
		void createAODS();
		void createDenoiseDS();
		void createPipelines(const DescriptorSetLayout::Ptr& frameSetLayout);
		void depthMipPrefilter(const CommandBuffer::Ptr& cmd, Camera camera, int frameIndex);
		void ambientOcclusion(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, Camera camera, int frameIndex);
		void denoiseAO(const CommandBuffer::Ptr& cmd, int frameIndex);
		void updateBuffer(const DescriptorSet::Ptr& descriptorSet, int frameIndex);
	public:
		GTAOParams mGTAO;

	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		int mFrameCount{ 0 };
		int mDenoisePasses{ 1 };
		Device::Ptr mDevice{ nullptr };
		Texture::Ptr mGDepth{ nullptr };
		Texture::Ptr mDepthMIPFilter{ nullptr };
		std::vector<VkImageView> mDepthMIPViews;
		uint32_t mMipCount{ 1 };
		ComputePipeline::Ptr mDepthFilterPipeline{ nullptr };
		UniformParameter::Ptr    mGDepthParam{ nullptr };
		UniformParameter::Ptr    mMip0Param{ nullptr };
		UniformParameter::Ptr    mMip1Param{ nullptr };
		UniformParameter::Ptr    mMip2Param{ nullptr };
		UniformParameter::Ptr    mMip3Param{ nullptr };
		UniformParameter::Ptr    mMip4Param{ nullptr };
		UniformParameter::Ptr    mGTAOParam{ nullptr };
		DescriptorSetLayout::Ptr mDepthFilterSetLayout{ nullptr };
		DescriptorPool::Ptr      mDepthFilterDescriptorPool{ nullptr };
		DescriptorSet::Ptr mDepthFilterDescriptorSets{ nullptr };

		Texture::Ptr mGNormal{ nullptr };
		Texture::Ptr mHilbertLUT{ nullptr };
		Texture::Ptr mAORaw{ nullptr };
		Texture::Ptr mEdges{ nullptr };
		UniformParameter::Ptr    mAODepthParam{ nullptr };
		UniformParameter::Ptr    mAONormalParam{ nullptr };
		UniformParameter::Ptr    mAOHilbertParam{ nullptr };
		UniformParameter::Ptr    mAOAORawParam{ nullptr };
		UniformParameter::Ptr    mEdgesParam{ nullptr };
		DescriptorSetLayout::Ptr mAOSetLayout{ nullptr };
		DescriptorPool::Ptr      mAODescriptorPool{ nullptr };
		DescriptorSet::Ptr mAODescriptorSets{ nullptr };
		ComputePipeline::Ptr mAOPipeline{ nullptr };

		Texture::Ptr mAO[2]{ nullptr, nullptr };
		uint32_t mAOPingPongIndex = 0;
		UniformParameter::Ptr mDenoiseAOSrcParam;
		UniformParameter::Ptr mDenoiseEdgesParam;
		UniformParameter::Ptr mDenoiseAOOutParam;
		DescriptorSetLayout::Ptr mDenoiseSetLayout;
		DescriptorPool::Ptr      mDenoiseDescriptorPool;
		std::vector<DescriptorSet::Ptr>    mDenoiseDescriptorSets{};
		ComputePipeline::Ptr mDenoisePipeline{ nullptr };
	};
}