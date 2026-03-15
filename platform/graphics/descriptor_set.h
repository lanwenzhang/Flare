#pragma once

#include "../../common.h"
#include "device.h"
#include "description.h"
#include "descriptor_set_layout.h"
#include "descriptor_pool.h"

namespace flare::vk {

	class DescriptorSet {
	public:

		using Ptr = std::shared_ptr<DescriptorSet>;
		static Ptr create(const Device::Ptr& device, const std::vector<UniformParameter::Ptr> params, const DescriptorSetLayout::Ptr& layout,const DescriptorPool::Ptr& pool,int frameCount) {
			
			return std::make_shared<DescriptorSet>(device, params, layout, pool, frameCount);
		}

		DescriptorSet(const Device::Ptr& device, const std::vector<UniformParameter::Ptr> params, const DescriptorSetLayout::Ptr& layout, const DescriptorPool::Ptr& pool,int frameCount);
		~DescriptorSet();

		void updateImage(const VkDescriptorSet& descriptorSet, uint32_t binding, const VkDescriptorImageInfo& imageInfo);
		void updateStorageImage(const VkDescriptorSet& descriptorSet, uint32_t binding, const VkDescriptorImageInfo& imageInfo);
		void updateBuffer(const VkDescriptorSet& descriptorSet, uint32_t binding, const VkDescriptorBufferInfo& bufferInfo);
		[[nodiscard]] auto getDescriptorSet(int frameCount) const { return mDescriptorSets[frameCount]; }

	private:
		Device::Ptr mDevice{ nullptr };
		std::vector<VkDescriptorSet> mDescriptorSets{};
		std::unordered_map<uint32_t, std::vector<VkDescriptorImageInfo>> mImageInfos;
	};
}