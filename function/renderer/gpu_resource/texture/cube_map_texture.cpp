#include "cube_map_texture.h"
#include <ktx.h>
#include <ktxvulkan.h>

namespace flare::renderer {

	CubeMapTexture::CubeMapTexture(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const std::string& ktxFilePath) {

		mDevice = device;
		mCommandPool = commandPool;

		// 1 load ktx
		ktxTexture* texture;
		KTX_error_code result = ktxTexture_CreateFromNamedFile(ktxFilePath.c_str(),
			KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
		if (result != KTX_SUCCESS) {
			throw std::runtime_error("Failed to load .ktx cubemap: " + ktxFilePath);
		}

		// 2 check if it is cube map
		if (!(texture->numDimensions == 2 && texture->isCubemap)) {
			ktxTexture_Destroy(texture);
			throw std::runtime_error("Provided .ktx is not a cubemap: " + ktxFilePath);
		}

		// 3 Create vulkan image
		ktxVulkanDeviceInfo vkdi;
		result = ktxVulkanDeviceInfo_Construct(&vkdi, mDevice->getPhysicalDevice(), mDevice->getDevice(), mDevice->getGraphicQueue(), mCommandPool->getCommandPool(), nullptr);
		if (result != KTX_SUCCESS) {
			ktxTexture_Destroy(texture);
			throw std::runtime_error("Failed to construct VulkanDeviceInfo for KTX");
		}

		ktxVulkanTexture vkTex;
		result = ktxTexture_VkUploadEx(texture, &vkdi, &vkTex, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (result != KTX_SUCCESS) {
			ktxVulkanDeviceInfo_Destruct(&vkdi);
			ktxTexture_Destroy(texture);
			throw std::runtime_error("Failed to upload KTX texture to Vulkan");
		}

		// 4 create cubemap imageView
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vkTex.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.format = vkTex.imageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = texture->numLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 6;
		viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

		VkImageView cubeView = VK_NULL_HANDLE;
		if (vkCreateImageView(device->getDevice(), &viewInfo, nullptr, &cubeView) != VK_SUCCESS) {
			ktxVulkanDeviceInfo_Destruct(&vkdi);
			ktxTexture_Destroy(texture);
			throw std::runtime_error("Failed to create cubemap VkImageView");
		}

		// 5 create image
		mCubeMapImage = flare::vk::Image::create(device, vkTex.deviceMemory, vkTex.image, cubeView, vkTex.imageFormat);
		mWidth = texture->baseWidth;
		mHeight = texture->baseHeight;
		mSampler = flare::vk::Sampler::create(mDevice);
		mImageInfo.imageLayout = mCubeMapImage->getLayout();
		mImageInfo.imageView = mCubeMapImage->getImageView();
		mImageInfo.sampler = mSampler->getSampler();

		// 6 clean up
		ktxTexture_Destroy(texture);
		ktxVulkanDeviceInfo_Destruct(&vkdi);
	}

	CubeMapTexture::CubeMapTexture(const Device::Ptr& device, const Image::Ptr& image){
		mDevice = device;
		mCubeMapImage = image;
		mSampler = flare::vk::Sampler::create(mDevice);
		mImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mImageInfo.imageView = mCubeMapImage->getImageView();
		mImageInfo.sampler = mSampler->getSampler();
	}

	CubeMapTexture::~CubeMapTexture(){}
}



