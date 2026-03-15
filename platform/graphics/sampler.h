#pragma once

#include "../../common.h"
#include "device.h"

namespace flare::vk {

    struct SamplerDesc {
        VkFilter minFilter = VK_FILTER_LINEAR;
        VkFilter magFilter = VK_FILTER_LINEAR;
        VkSamplerAddressMode addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkBool32 anisotropyEnable = VK_TRUE;
        float maxAnisotropy = 16.0f;
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        float mipLodBias = 0.0f;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        VkBool32 compareEnable = VK_FALSE;
        VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
        VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        VkBool32 unnormalizedCoordinates = VK_FALSE;

        bool isDefault() const {
            return minFilter == VK_FILTER_LINEAR &&
                magFilter == VK_FILTER_LINEAR &&
                addressU == VK_SAMPLER_ADDRESS_MODE_REPEAT &&
                addressV == VK_SAMPLER_ADDRESS_MODE_REPEAT &&
                addressW == VK_SAMPLER_ADDRESS_MODE_REPEAT &&
                anisotropyEnable == VK_TRUE &&
                maxAnisotropy == 16.0f &&
                mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR &&
                compareEnable == VK_FALSE;
                mipLodBias == 0.0f &&
                minLod == 0.0f &&
                maxLod == 0.0f;
        }
    };


	class Sampler {
	public:
		using Ptr = std::shared_ptr<Sampler>;
		static Ptr create(const Device::Ptr& device) { return std::make_shared<Sampler>(device); }
        static Ptr create(const Device::Ptr& device, const SamplerDesc& desc) {
            return std::make_shared<Sampler>(device, desc);
        }

		Sampler(const Device::Ptr& device);
        Sampler(const Device::Ptr& device, const SamplerDesc& desc);
		~Sampler();
		[[nodiscard]] auto getSampler() const { return mSampler; }
	private:
		Device::Ptr mDevice{ nullptr };
		VkSampler mSampler{ VK_NULL_HANDLE };
	};
}