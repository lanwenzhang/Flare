#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_pool.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../gpu_resource/buffer/geometry_buffer.h"
#include "../gpu_resource/texture/texture.h" 
#include "../frame_graph.h"

namespace flare::renderer {
	using namespace flare::vk;

	class TAAPass {
	public:
		using Ptr = std::shared_ptr<TAAPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<TAAPass>(device); }

		TAAPass(Device::Ptr& device) { mDevice = device; }
		~TAAPass() = default;

		void init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const Texture::Ptr& depthTex, const Texture::Ptr& lightingTex, const Texture::Ptr& motionVectorTex,
				  const DescriptorSetLayout::Ptr& frameSetLayout, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, const Texture::Ptr& lighting, const DescriptorSet::Ptr& frameSet, int frameIndex);
		Texture::Ptr getCurrentHistoryTexture() const {
			return (mCurrentHistoryIndex == 0) ? mHistory0 : mHistory1;
		}

	private:
		void createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph);
		void createDescriptor(const Texture::Ptr& depthTex, const Texture::Ptr& lightingTex,
							  const Texture::Ptr& motionVectorTex, int frameCount);
		void createPipeline(const DescriptorSetLayout::Ptr& frameSetLayout);

	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };

		Device::Ptr          mDevice{ nullptr };
		ComputePipeline::Ptr mPipeline{ nullptr };

		UniformParameter::Ptr mDepthParam{ nullptr };
		UniformParameter::Ptr mMotionVectorParam{ nullptr };
		UniformParameter::Ptr mCurrentColorParam{ nullptr };
		UniformParameter::Ptr mHistoryReadParam{ nullptr };
		UniformParameter::Ptr mHistoryWriteParam{ nullptr };

		DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet{ nullptr };

		Texture::Ptr mHistory0{ nullptr };
		Texture::Ptr mHistory1{ nullptr };

		int mPreviousHistoryIndex{ 0 };
		int mCurrentHistoryIndex{ 0 };
	};

}