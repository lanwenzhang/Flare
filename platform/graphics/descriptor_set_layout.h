#pragma once

#include "../../common.h"
#include "device.h"
#include "description.h"

namespace flare::vk {

	class DescriptorSetLayout {
	public:

		using Ptr = std::shared_ptr<DescriptorSetLayout>;
		static Ptr create(const Device::Ptr& device) { return std::make_shared<DescriptorSetLayout>(device); }

		DescriptorSetLayout(const Device::Ptr& device);
		~DescriptorSetLayout();

		void build(const std::vector<UniformParameter::Ptr>& params);

		[[nodiscard]] const VkDescriptorSetLayout& getLayout() const { return mLayout; }

	private:

		Device::Ptr mDevice{ nullptr };
		VkDescriptorSetLayout mLayout{ VK_NULL_HANDLE };
		std::vector<UniformParameter::Ptr> mParams{};
	};
}