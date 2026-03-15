#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/swap_chain.h"
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
#include "../../scene/light/directional_light.h"

namespace flare::renderer {

	using namespace flare::vk;

	class GeometryPass {
	public:
		using Ptr = std::shared_ptr<GeometryPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<GeometryPass>(device); }

		GeometryPass(Device::Ptr& device) { mDevice = device; }
		~GeometryPass() = default;

		void init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
				  const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout);
		void render(const CommandBuffer::Ptr& cmd, const GeometryBuffer::Ptr& geoBuffer,
					const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet,
					Camera camera, DirectionalLight light, int frameIndex);
	public:
		[[nodiscard]] auto getGbufferColor() { return mGColor; };
		[[nodiscard]] auto getGbufferNormal() { return mGNormal; };
		[[nodiscard]] auto getGbufferMR() { return mGMR; };
		[[nodiscard]] auto getGbufferDepth() { return mGDepth; };
		[[nodiscard]] auto getPrevGbufferNormal() { return mPrevGNormal; };
		[[nodiscard]] auto getPrevGbufferDepth() { return mPrevGDepth; };

	private:
		void createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph);
		void createPipeline(const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
							const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout);
	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		Device::Ptr mDevice{ nullptr };
		Pipeline::Ptr mPipeline{ nullptr };

		Texture::Ptr mPrevGNormal{ nullptr };
		Texture::Ptr mPrevGDepth{ nullptr };
		Texture::Ptr mGColor{ nullptr };
		Texture::Ptr mGNormal{ nullptr };
		Texture::Ptr mGMR{ nullptr };
		Texture::Ptr mGDepth{ nullptr };

		Framebuffer::Ptr mGBufferFb{ nullptr };
	};
}



