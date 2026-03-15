#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace flare::renderer {

	Texture::Texture(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::string& imageFilePath, 
					 VkFormat format, const SamplerDesc& samplerDesc) {
		mDevice = device;
		int texWidth, texHeight, texSize, texChannles;
		stbi_uc* pixels = stbi_load(imageFilePath.c_str(), &texWidth, &texHeight, &texChannles, STBI_rgb_alpha);
		if (!pixels) { throw std::runtime_error("Error: failed to read image data");}
		texSize = texWidth * texHeight * 4;
		uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		mImage = flare::vk::Image::create(mDevice, texWidth, texHeight, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
									      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			                              VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
										  0, 1, VK_IMAGE_VIEW_TYPE_2D,mipLevels);


		VkImageSubresourceRange region{};
		region.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.baseArrayLayer = 0;
		region.layerCount = 1;
		region.baseMipLevel = 0;
		region.levelCount = 1;
		region.levelCount = mipLevels;

		mImage->setImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, region, commandPool);
		//mImage->fillImageData(texSize, (void*)pixels, commandPool, 0);
		//mImage->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, region, commandPool);

		mImage->fillImageData(texSize, pixels, commandPool, 0);
		mImage->generateMipmaps(commandPool);
		stbi_image_free(pixels);

		SamplerDesc finalDesc = samplerDesc;
		uint32_t mips = mImage->getMipLevels();
		if (finalDesc.maxLod == 0.0f && mips > 1) {
			finalDesc.minLod = 0.0f;
			finalDesc.maxLod = float(mips - 1);
			finalDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}

		if (finalDesc.isDefault()) mSampler = flare::vk::Sampler::create(mDevice);
		else mSampler = flare::vk::Sampler::create(mDevice, finalDesc);
		mImageInfo.imageLayout = mImage->getLayout();
		mImageInfo.imageView = mImage->getImageView();
		mImageInfo.sampler = mSampler->getSampler();
	}

	Texture::Texture(const flare::vk::Device::Ptr& device, const flare::vk::Image::Ptr& image, const SamplerDesc& samplerDesc){
		mDevice = device;
		mImage = image;

		SamplerDesc finalDesc = samplerDesc;
		uint32_t mips = mImage->getMipLevels();
		if (finalDesc.maxLod == 0.0f && mips > 1) {
			finalDesc.minLod = 0.0f;
			finalDesc.maxLod = float(mips - 1);
			finalDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}

		if (finalDesc.isDefault()) mSampler = flare::vk::Sampler::create(mDevice);
		else mSampler = flare::vk::Sampler::create(mDevice, finalDesc);
		mImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mImageInfo.imageView = mImage->getImageView();
		mImageInfo.sampler = mSampler->getSampler();
	}

	Texture::Texture(const Device::Ptr& device, const CommandPool::Ptr& commandPool, uint32_t width, uint32_t height,
					 VkFormat format, const void* data, size_t dataSizeBytes, const SamplerDesc& samplerDesc) {
		
		mDevice = device;
		mImage = flare::vk::Image::create(mDevice, (int)width, (int)height, format, VK_IMAGE_TYPE_2D,
										 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
										 VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										 VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_VIEW_TYPE_2D, 1);

		VkImageSubresourceRange region{};
		region.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.baseArrayLayer = 0;
		region.layerCount = 1;
		region.baseMipLevel = 0;
		region.levelCount = 1;

		mImage->setImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							   VK_PIPELINE_STAGE_TRANSFER_BIT, region, commandPool);
		mImage->fillImageData(dataSizeBytes, const_cast<void*>(data), commandPool, 0);
		mImage->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, region, commandPool);

		SamplerDesc finalDesc = samplerDesc;
		if (finalDesc.isDefault())
			mSampler = flare::vk::Sampler::create(mDevice);
		else
			mSampler = flare::vk::Sampler::create(mDevice, finalDesc);

		mImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mImageInfo.imageView = mImage->getImageView();
		mImageInfo.sampler = mSampler->getSampler();
	}
}