#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../gpu_resource/buffer/geometry_buffer.h"
#include "../gpu_resource/texture/texture.h" 
#include "../frame_graph.h"

namespace flare::renderer {

	class MotionVectorPass {
	public:
		using Ptr = std::shared_ptr<MotionVectorPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<MotionVectorPass>(device); }
		
		MotionVectorPass(Device::Ptr& device) { mDevice = device; }
		~MotionVectorPass() = default;
		void init(const Texture::Ptr& depthTex, const FrameGraph& frameGraph, const DescriptorSetLayout::Ptr& frameSetLayout, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, int frameIndex);
		[[nodiscard]] auto getMotionVectors() const { return mMotionVectors; }
	private:
		void createImages(const FrameGraph& frameGraph);
		void createDescriptor(const Texture::Ptr& depthTex, int frameCount);
		void createPipeline(const DescriptorSetLayout::Ptr& frameSetLayout);

	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		Device::Ptr mDevice{ nullptr };
		Texture::Ptr mMotionVectors{ nullptr };
		ComputePipeline::Ptr mPipeline{ nullptr };
		UniformParameter::Ptr mDepthParam{ nullptr };
		UniformParameter::Ptr  mMotionVectorParam{ nullptr };
		DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet{ nullptr };
	};
}