#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/swap_chain.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../gpu_resource/texture/texture.h"

namespace flare::renderer {

	using namespace flare::vk;
	class ToneMappingPass {
	public:
		using Ptr = std::shared_ptr<ToneMappingPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<ToneMappingPass>(device); }

		ToneMappingPass(Device::Ptr& device) { mDevice = device;}
		~ToneMappingPass() = default;

		void init(const SwapChain::Ptr& swapChain, const Texture::Ptr& colorTexture, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, const SwapChain::Ptr& swapChain, const Texture::Ptr& colorTexture, uint32_t imageIndex, int frameIndex);

	private: 
		void createPipeline(const SwapChain::Ptr& swapChain);
		void createDescriptorSet();

	private:
		int mFrameCount{ 0 };
		Device::Ptr mDevice{ nullptr };
		Pipeline::Ptr mPipeline{ nullptr };
		std::vector<UniformParameter::Ptr> mParams{};
		DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet{ nullptr };

		Texture::Ptr mColor{ nullptr };

	};
}