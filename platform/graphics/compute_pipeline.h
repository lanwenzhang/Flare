#pragma once

#include "../../common.h"
#include "device.h"
#include "shader.h"

namespace flare::vk {

	class ComputePipeline {
	public:
		using Ptr = std::shared_ptr<ComputePipeline>;
		static Ptr create(const Device::Ptr& device) { return std::make_shared<ComputePipeline>(device); }

		ComputePipeline(const Device::Ptr& device);
		~ComputePipeline();

		void setShader(const Shader::Ptr& computeShader);
		void setDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& layouts) { mSetLayouts = layouts; }
		void setSpecializationConstant(uint32_t constantId, size_t size, const void* data);
		void setPushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags stageFlags);
		void build();

		[[nodiscard]] auto getPipeline() const { return mComputePipeline; }
		[[nodiscard]] auto getLayout() const { return mLayout; }

	private:
		Device::Ptr mDevice{ nullptr };
		Shader::Ptr mComputeShader{ nullptr };
		VkPipeline  mComputePipeline{ VK_NULL_HANDLE };
		VkPipelineLayout mLayout{ VK_NULL_HANDLE };
		std::vector<VkDescriptorSetLayout> mSetLayouts{};
		std::vector<VkSpecializationMapEntry> mSpecEntries{};
		std::vector<uint8_t> mSpecData{};
		bool mHasCustomPushConstantRange{ false };
		VkPushConstantRange mCustomPushConstantRange{};
	};
}

