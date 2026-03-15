#pragma once

#include "../../../../common.h"
#include "../../../../platform/graphics/image.h"
#include "../../../../platform/graphics/sampler.h"
#include "../../../../platform/graphics/device.h"
#include "../../../../platform/graphics/command_pool.h"


namespace flare::renderer {

	using namespace flare::vk;

	class Texture {
	public:
		using Ptr = std::shared_ptr<Texture>;
		static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::string& imageFilePath, 
						  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB, const SamplerDesc& samplerDesc = {}) {
			return std::make_shared<Texture>(device, commandPool, imageFilePath, format, samplerDesc);
		}

		static Ptr create(const Device::Ptr& device, const Image::Ptr& image, const SamplerDesc& samplerDesc = {}) {
			return std::make_shared<Texture>(device, image, samplerDesc);
		}

		static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, uint32_t width, uint32_t height,
						  VkFormat format, const void* data, size_t dataSizeBytes, const SamplerDesc& samplerDesc = {}) {
			return std::make_shared<Texture>(device, commandPool, width, height, format, data, dataSizeBytes, samplerDesc);
		}
		Texture(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::string& imageFilePath, 
				VkFormat format = VK_FORMAT_R8G8B8A8_SRGB, const SamplerDesc& samplerDesc = {});
		Texture(const Device::Ptr& device, const Image::Ptr& image, const SamplerDesc& samplerDesc = {});
		Texture(const Device::Ptr& device, const CommandPool::Ptr& commandPool, uint32_t width, uint32_t height,
				VkFormat format, const void* data, size_t dataSizeBytes, const SamplerDesc& samplerDesc = {});
		~Texture() = default;	

		[[nodiscard]] auto getImage() const { return mImage; }
		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }
		[[nodiscard]] VkImageView getImageView() const { return mImageInfo.imageView; }
		[[nodiscard]] VkSampler getSampler() const { return mImageInfo.sampler; }

	private:
		Device::Ptr mDevice{ nullptr };
		Image::Ptr mImage{ nullptr };
		Sampler::Ptr mSampler{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
	};
}