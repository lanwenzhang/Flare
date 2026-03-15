#include "pipeline.h"

namespace flare::vk {

	Pipeline::Pipeline(const Device::Ptr& device, const RenderPass::Ptr& renderPass){
	
		mDevice = device;
		mRenderPass = renderPass;

		mVertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mSampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		mBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		mDepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		mLayoutState.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		mLayoutState.flags = VK_PIPELINE_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	}

	Pipeline::Pipeline(const Device::Ptr& device) {

		mDevice = device;
		mVertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mSampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		mBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		mDepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		mLayoutState.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		mLayoutState.flags = VK_PIPELINE_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	}


	Pipeline::~Pipeline(){

		if (mLayout != VK_NULL_HANDLE) {

			vkDestroyPipelineLayout(mDevice->getDevice(), mLayout, nullptr);
		}


		if (mPipeline != VK_NULL_HANDLE) {

			vkDestroyPipeline(mDevice->getDevice(), mPipeline, nullptr);
		}
	
	}

	void Pipeline::setDynamicStates(const std::vector<VkDynamicState>& dynamicStates) {
		mDynamicStatesStorage = dynamicStates;

		mDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		mDynamicState.dynamicStateCount = static_cast<uint32_t>(mDynamicStatesStorage.size());
		mDynamicState.pDynamicStates = mDynamicStatesStorage.data();
	}

	void Pipeline::setShaderGroup(const std::vector<Shader::Ptr>& shaderGroup) {
	
		mShaders = shaderGroup;
	
	}

	void Pipeline::build() {

		// 1 Create shader
		std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfos{};
		for (const auto& shader : mShaders) {

			VkPipelineShaderStageCreateInfo shaderCreateInfo{};
			shaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderCreateInfo.stage = shader->getShaderStage();
			shaderCreateInfo.pName = shader->getShaderEntryPoint().c_str();
			shaderCreateInfo.module = shader->getShaderModule();

			shaderCreateInfos.push_back(shaderCreateInfo);

		}
		
		if (mDynamicState.dynamicStateCount > 0) {
			mViewportState.viewportCount = 1;
			mViewportState.pViewports = nullptr;
			mViewportState.scissorCount = 1;
			mViewportState.pScissors = nullptr;
		}
		else {
			mViewportState.viewportCount = static_cast<uint32_t>(mViewports.size());
			mViewportState.pViewports = mViewports.data();
			mViewportState.scissorCount = static_cast<uint32_t>(mScissors.size());
			mViewportState.pScissors = mScissors.data();
		}


		mBlendState.attachmentCount = static_cast<uint32_t>(mBlendAttachmentStates.size());
		mBlendState.pAttachments = mBlendAttachmentStates.data();

		// 4 Create layout
		if (mLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(mDevice->getDevice(), mLayout, nullptr);
		}



		if (vkCreatePipelineLayout(mDevice->getDevice(), &mLayoutState, nullptr, &mLayout) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create pipeline layout");
		}


		// 5 Create pipeline
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderCreateInfos.size());
		pipelineCreateInfo.pStages = shaderCreateInfos.data();

		pipelineCreateInfo.pVertexInputState = &mVertexInputState;
		pipelineCreateInfo.pInputAssemblyState = &mAssemblyState;
		pipelineCreateInfo.pViewportState = &mViewportState;
		pipelineCreateInfo.pRasterizationState = &mRasterState;
		pipelineCreateInfo.pMultisampleState = &mSampleState;
		pipelineCreateInfo.pDepthStencilState = &mDepthStencilState;
		pipelineCreateInfo.pColorBlendState = &mBlendState;
		pipelineCreateInfo.pDynamicState = (mDynamicState.dynamicStateCount > 0) ? &mDynamicState : nullptr;
		pipelineCreateInfo.layout = mLayout;

		if (!mRenderPass) {
			VkPipelineRenderingCreateInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

			if (!mColorAttachmentFormats.empty()) {
				renderingInfo.colorAttachmentCount = static_cast<uint32_t>(mColorAttachmentFormats.size());
				renderingInfo.pColorAttachmentFormats = mColorAttachmentFormats.data();
			}
			else {
				renderingInfo.colorAttachmentCount = 0;
				renderingInfo.pColorAttachmentFormats = nullptr;
			}
			renderingInfo.depthAttachmentFormat = mDepthAttachmentFormat;
			renderingInfo.stencilAttachmentFormat = mStencilAttachmentFormat;
			pipelineCreateInfo.pNext = &renderingInfo;
		}
		else {
			pipelineCreateInfo.renderPass = mRenderPass->getRenderPass();
			pipelineCreateInfo.subpass = 0;
			pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		}

		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;


		if (mPipeline != VK_NULL_HANDLE) {
			
			vkDestroyPipeline(mDevice->getDevice(), mPipeline, nullptr);
		}

		if (vkCreateGraphicsPipelines(mDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mPipeline) != VK_SUCCESS) {
			
			throw std::runtime_error("Error:failed to create pipeline");

		}

	}

}