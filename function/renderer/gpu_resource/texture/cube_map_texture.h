#pragma once

#include "../../../../common.h"
#include "../../../../platform/graphics/image.h"
#include "../../../../platform/graphics/sampler.h"
#include "../../../../platform/graphics/device.h"
#include "../../../../platform/graphics/command_pool.h"

namespace flare::renderer {

	using namespace flare::vk;

	class CubeMapTexture {
	public:
		using Ptr = std::shared_ptr<CubeMapTexture>;
		static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::string& ktxFilePath) {
			return std::make_shared<CubeMapTexture>(device, commandPool, ktxFilePath);
		};

		static Ptr create(const Device::Ptr& device, const Image::Ptr& image) {
			return std::make_shared<CubeMapTexture>(device, image);
		}

		CubeMapTexture(const Device::Ptr& device,const CommandPool::Ptr& commandPool, const std::string& ktxFilePath);
		CubeMapTexture(const Device::Ptr& device, const Image::Ptr& image);
		~CubeMapTexture();

		[[nodiscard]] Image::Ptr getImage(){ return mCubeMapImage; }
		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }
		[[nodiscard]] VkImageView getImageView() const { return mImageInfo.imageView; }
		[[nodiscard]] VkSampler getSampler() const { return mImageInfo.sampler; }
		[[nodiscard]] auto getWidth() const { return mWidth; }
		[[nodiscard]] auto getHeight() const { return mHeight; }

	private:
		unsigned int mWidth{};
		unsigned int mHeight{};

		Device::Ptr mDevice{ nullptr };
		CommandPool::Ptr mCommandPool{ nullptr };
		Image::Ptr mCubeMapImage{ nullptr };
		Sampler::Ptr mSampler{ nullptr };

		VkDescriptorImageInfo mImageInfo{};
	};
}