#include "compute_pipeline.h"

namespace flare::vk {

	ComputePipeline::ComputePipeline(const Device::Ptr& device){
		mDevice = device;
	}

	void ComputePipeline::setShader(const Shader::Ptr& computeShader){
		mComputeShader = computeShader;
	}

	void ComputePipeline::setSpecializationConstant(uint32_t constantId, size_t size, const void* data) {
		VkSpecializationMapEntry entry{};
		entry.constantID = constantId;
		entry.offset = static_cast<uint32_t>(mSpecData.size());
		entry.size = size;
		mSpecEntries.push_back(entry);

		const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
		mSpecData.insert(mSpecData.end(), bytes, bytes + size);
	}

	void ComputePipeline::setPushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags stageFlags) {
		mHasCustomPushConstantRange = true;
		mCustomPushConstantRange.offset = offset;
		mCustomPushConstantRange.size = size;
		mCustomPushConstantRange.stageFlags = stageFlags;
	}

	void ComputePipeline::build(){
		if (!mComputeShader) {
			throw std::runtime_error("Compute shader is not set");
		}
		VkPushConstantRange defaultRange{};
		defaultRange.offset = 0;
		defaultRange.size = sizeof(glm::mat4) + sizeof(uint64_t) * 2 + sizeof(float) + sizeof(uint32_t);
		defaultRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		const VkPushConstantRange* rangePtr = nullptr;
		if (mHasCustomPushConstantRange) {
			rangePtr = &mCustomPushConstantRange;
		}
		else {
			rangePtr = &defaultRange;
		}

		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = static_cast<uint32_t>(mSetLayouts.size());
		layoutInfo.pSetLayouts = mSetLayouts.data();
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = rangePtr;

		if (vkCreatePipelineLayout(mDevice->getDevice(), &layoutInfo, nullptr, &mLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create compute pipeline layout");
		}

		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageInfo.module = mComputeShader->getShaderModule();
		shaderStageInfo.pName = mComputeShader->getShaderEntryPoint().c_str();

		VkSpecializationInfo specInfo{};
		if (!mSpecEntries.empty()) {
			specInfo.mapEntryCount = static_cast<uint32_t>(mSpecEntries.size());
			specInfo.pMapEntries = mSpecEntries.data();
			specInfo.dataSize = mSpecData.size();
			specInfo.pData = mSpecData.data();
			shaderStageInfo.pSpecializationInfo = &specInfo;
		}
		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.stage = shaderStageInfo;
		pipelineInfo.layout = mLayout;
		if (vkCreateComputePipelines(mDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mComputePipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create compute pipeline");
		}
	}

	ComputePipeline::~ComputePipeline() {
		if (mComputePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(mDevice->getDevice(), mComputePipeline, nullptr);
		}
		if (mLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(mDevice->getDevice(), mLayout, nullptr);
		}
	}
}

