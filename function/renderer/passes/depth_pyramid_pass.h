#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/descriptor_set_layout.h"
#include "../../../platform/graphics/descriptor_pool.h"
#include "../../../platform/graphics/image.h"
#include "../frame_graph.h"
#include "../gpu_resource/texture/texture.h"

namespace flare::renderer {

	class DepthPyramidPass {
	public:
		using Ptr = std::shared_ptr<DepthPyramidPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<DepthPyramidPass>(device); }
		DepthPyramidPass(Device::Ptr& device) { mDevice = device; }
		~DepthPyramidPass();

		void init(const FrameGraph& frameGraph, const Texture::Ptr& preDepth, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, int frameIndex);
		[[nodiscard]] auto getHZB() { return mHZB; }

	private:
		void createImages(const FrameGraph& frameGraph);
		void createDescriptors(int frameCount);
		void createPipeline();

	private:
		uint32_t mWidth{ 1200 };
		uint32_t mHeight{ 800 };
		uint32_t mMipCount{ 1 };

		Device::Ptr mDevice{ nullptr };
		ComputePipeline::Ptr mPipeline{ nullptr };

		DescriptorSetLayout::Ptr mSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		std::vector<DescriptorSet::Ptr> mDescriptorSets{};
		UniformParameter::Ptr    mSrcParam{ nullptr };
		UniformParameter::Ptr    mDstParam{ nullptr }; 

		Texture::Ptr mPreDepth{ nullptr };
		Texture::Ptr mHZB{ nullptr };
		std::vector<VkImageView> mHZBMipViews;
	};
}