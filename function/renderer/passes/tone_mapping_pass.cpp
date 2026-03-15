#include "tone_mapping_pass.h"

namespace flare::renderer {

	void ToneMappingPass::init(const SwapChain::Ptr& swapChain, const Texture::Ptr& colorTexture, int frameCount) {
		mColor = colorTexture;
		mFrameCount = frameCount;
		createDescriptorSet();
		createPipeline(swapChain);
	}

	void ToneMappingPass::createPipeline(const SwapChain::Ptr& swapChain) {
		
		mPipeline = Pipeline::create(mDevice);
	
		// attachment format
		mPipeline->setColorAttachmentFormats({ swapChain->getFormat() });
		VkFormat depthFormat = swapChain->getDepthImageFormat();
		mPipeline->setDepthAttachmentFormat(depthFormat);
		if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
			mPipeline->setStencilAttachmentFormat(depthFormat);
		}

		// viewport
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
		mPipeline->setDynamicStates(dynamicStates);

		// shader
		std::vector<Shader::Ptr> shaderGroup{};
		shaderGroup.push_back(Shader::create(mDevice, "shaders/tone_mapping/quad_flip_vs.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"));
		shaderGroup.push_back(Shader::create(mDevice, "shaders/tone_mapping/combine_fs.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"));
		mPipeline->setShaderGroup(shaderGroup);

		// vertex
		mPipeline->mVertexInputState.vertexBindingDescriptionCount = 0;
		mPipeline->mVertexInputState.pVertexBindingDescriptions = nullptr;
		mPipeline->mVertexInputState.vertexAttributeDescriptionCount = 0;
		mPipeline->mVertexInputState.pVertexAttributeDescriptions = nullptr;

		// assembly
		mPipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mPipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		mPipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		// rasterization
		mPipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mPipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;
		mPipeline->mRasterState.lineWidth = 1.0f;
		mPipeline->mRasterState.cullMode = VK_CULL_MODE_NONE;
		mPipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		mPipeline->mRasterState.depthBiasEnable = VK_FALSE;
		mPipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		mPipeline->mRasterState.depthBiasClamp = 0.0f;
		mPipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		// msaa
		mPipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		mPipeline->mSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		mPipeline->mSampleState.minSampleShading = 1.0f;
		mPipeline->mSampleState.pSampleMask = nullptr;
		mPipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		mPipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		// depth stencil
		mPipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		mPipeline->mDepthStencilState.depthWriteEnable = VK_FALSE;
		mPipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;

		// blend
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachment.blendEnable = VK_FALSE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		mPipeline->pushBlendAttachments(blendAttachment);

		mPipeline->mBlendState.logicOpEnable = VK_FALSE;
		mPipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;
		mPipeline->mBlendState.blendConstants[0] = 0.0f;
		mPipeline->mBlendState.blendConstants[1] = 0.0f;
		mPipeline->mBlendState.blendConstants[2] = 0.0f;
		mPipeline->mBlendState.blendConstants[3] = 0.0f;
		
		// set layout
		mPipeline->mSetLayoutsStorage = { mDescriptorSetLayout->getLayout()};
		mPipeline->mLayoutState.setLayoutCount = static_cast<uint32_t>(mPipeline->mSetLayoutsStorage.size());
		mPipeline->mLayoutState.pSetLayouts = mPipeline->mSetLayoutsStorage.data();
		mPipeline->mLayoutState.flags = 0;
		mPipeline->mLayoutState.pushConstantRangeCount = 0;
		mPipeline->mLayoutState.pPushConstantRanges = nullptr;
		
		// build
		mPipeline->build();
	}

	void ToneMappingPass::createDescriptorSet() {
		auto param = UniformParameter::create();
		param->mBinding = 0;
		param->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param->mCount = 1;
		param->mImageInfos.resize(1);
		param->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param->mImageInfos[0].imageView = mColor->getImageView();
		param->mImageInfos[0].sampler = mColor->getSampler();
		mParams.push_back(param);

		mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout->build(mParams);
		mDescriptorPool = DescriptorPool::create(mDevice);
		mDescriptorPool->build(mParams, mFrameCount);
		mDescriptorSet = DescriptorSet::create(mDevice, mParams, mDescriptorSetLayout, mDescriptorPool, mFrameCount);

		for (int i = 0; i < mFrameCount; ++i) {
			mDescriptorSet->updateImage(mDescriptorSet->getDescriptorSet(i), param->mBinding, param->mImageInfos[0]);
		}
	}

	void ToneMappingPass::render(const CommandBuffer::Ptr& cmd, const SwapChain::Ptr& swapChain, const Texture::Ptr& colorTexture, uint32_t imageIndex, int frameIndex){
		
		cmd->transitionImageLayout(swapChain->getImage(imageIndex), swapChain->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = colorTexture->getImage()->getImageView();
		info.sampler = colorTexture->getSampler();

		auto ds = mDescriptorSet->getDescriptorSet(frameIndex);
		mDescriptorSet->updateImage(ds, 0, info);

		cmd->beginRendering(swapChain, imageIndex);
		const uint32_t width = swapChain->getExtent().width;
		const uint32_t height = swapChain->getExtent().height;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width);
		viewport.height = static_cast<float>(height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd->setViewport(0, viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { width, height };
		cmd->setScissor(0, scissor);

		cmd->bindGraphicPipeline(mPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 0);
		cmd->draw(3);
		cmd->endRendering();
	}
}