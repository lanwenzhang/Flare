#include "sampler.h"

namespace flare::vk {

	Sampler::Sampler(const Device::Ptr& device) {	
		mDevice = device;

		VkSamplerCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		// Filter
		createInfo.magFilter = VK_FILTER_LINEAR;
		createInfo.minFilter = VK_FILTER_LINEAR;

		// Wrap
		createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		// Anisotropy
		createInfo.anisotropyEnable = VK_TRUE;
		createInfo.maxAnisotropy = 16;

		createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		createInfo.unnormalizedCoordinates = VK_FALSE;

		createInfo.compareEnable = VK_FALSE;
		createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		// Mipmap
		createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		createInfo.mipLodBias = 0.0f;
		createInfo.minLod = 0.0f;
		createInfo.maxLod = 0.0f;

		if (vkCreateSampler(mDevice->getDevice(), &createInfo, nullptr, &mSampler) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create sampler");
		}
	}

	Sampler::Sampler(const Device::Ptr& device, const SamplerDesc& d) {
		mDevice = device;

		VkSamplerCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		// Filter
		ci.magFilter = d.magFilter;
		ci.minFilter = d.minFilter;

		// Wrap
		ci.addressModeU = d.addressU;
		ci.addressModeV = d.addressV;
		ci.addressModeW = d.addressW;

		// Anisotropy
		ci.anisotropyEnable = d.anisotropyEnable;
		ci.maxAnisotropy = d.maxAnisotropy;

		// Border/coords
		ci.borderColor = d.borderColor;
		ci.unnormalizedCoordinates = d.unnormalizedCoordinates;

		// Compare 
		ci.compareEnable = d.compareEnable;
		ci.compareOp = d.compareOp;

		// Mipmap
		ci.mipmapMode = d.mipmapMode;
		ci.mipLodBias = d.mipLodBias;
		ci.minLod = d.minLod;
		ci.maxLod = d.maxLod;

		if (vkCreateSampler(mDevice->getDevice(), &ci, nullptr, &mSampler) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create sampler");
		}
	}

	Sampler::~Sampler() {
		if (mSampler != VK_NULL_HANDLE) {
			vkDestroySampler(mDevice->getDevice(), mSampler, nullptr);
		}
	}
}