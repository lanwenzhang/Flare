#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/descriptor_set_layout.h"
#include "../../../platform/graphics/image.h"
#include "../../../platform/graphics/framebuffer.h"
#include "../gpu_resource/buffer/geometry_buffer.h"
#include "../gpu_resource/texture/texture.h"
#include "../frame_graph.h"
#include "../../scene/camera/camera.h"

namespace flare::renderer {

	using namespace flare::vk;

	class DepthPrePass {
	public:
		using Ptr = std::shared_ptr<DepthPrePass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<DepthPrePass>(device); }

		DepthPrePass(Device::Ptr& device) { mDevice = device; }
		~DepthPrePass() = default;

		void init(const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
			      const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout);
		void render(const CommandBuffer::Ptr& cmd, const GeometryBuffer::Ptr& geoBuffer,
					const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet,
					Camera camera, int frameIndex);
		[[nodiscard]] auto getDepthPreLinear() {return mDepthPreLinear;}

	private:
		void createImages(const FrameGraph& frameGraph);
		void createPipeline(const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
			                const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout);
	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		Device::Ptr mDevice{ nullptr };
		Pipeline::Ptr mPipeline{ nullptr };
		Texture::Ptr mDepthPre{ nullptr };
		Texture::Ptr mDepthPreLinear{ nullptr };
		Framebuffer::Ptr mDepthPreFb{ nullptr };
	};
}