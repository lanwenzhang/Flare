#include "descriptor_set_layout.h"

namespace flare::vk {

	DescriptorSetLayout::DescriptorSetLayout(const Device::Ptr& device) {
		mDevice = device;
	}

	DescriptorSetLayout::~DescriptorSetLayout() {
		if (mLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(mDevice->getDevice(), mLayout, nullptr);
		}
	}

	void DescriptorSetLayout::build(const std::vector<UniformParameter::Ptr>& params) {
		mParams = params;
		if (mLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(mDevice->getDevice(), mLayout, nullptr);
		}

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
		std::vector<VkDescriptorBindingFlags> bindingFlags;

		for (const auto& param : mParams) {
			if (param->mCount == 0)
				continue;
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.descriptorType = param->mDescriptorType;
			layoutBinding.binding = param->mBinding;
			layoutBinding.stageFlags = param->mStage;
			layoutBinding.descriptorCount = (param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? 
											std::max(param->mCount, 1u): param->mCount;
			layoutBindings.push_back(layoutBinding);
			VkDescriptorBindingFlags flags = 0;
			if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && param->mCount > 1) {
				flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
				flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
				flags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
			}
			bindingFlags.push_back(flags);
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
		bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
		bindingFlagsInfo.pBindingFlags = bindingFlags.data();

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.pNext = &bindingFlagsInfo;
		createInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		createInfo.pBindings = layoutBindings.data();
		createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

		if (vkCreateDescriptorSetLayout(mDevice->getDevice(), &createInfo, nullptr, &mLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create descriptor set layout");
		}
	}
}