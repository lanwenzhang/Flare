#include "geometry_pass.h"

namespace flare::renderer {

	void GeometryPass::init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
							const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout) {
		createImages(commandPool,frameGraph);
		createPipeline(frameGraph, geometryBuffer, frameSetLayout, staticSetLayout);
	}

	void GeometryPass::createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph) {

		SamplerDesc pointClamp{};
		pointClamp.minFilter = VK_FILTER_NEAREST;
		pointClamp.magFilter = VK_FILTER_NEAREST;
		pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		pointClamp.anisotropyEnable = VK_FALSE;
		pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		SamplerDesc depthCompare = pointClamp;
		depthCompare.compareEnable = VK_TRUE;
		depthCompare.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		mGColor = Texture::create(mDevice, frameGraph.getImage("gbuffer_color"), pointClamp);
		mGNormal = Texture::create(mDevice, frameGraph.getImage("gbuffer_normal"), pointClamp);
		mGMR = Texture::create(mDevice, frameGraph.getImage("gbuffer_metallic_roughness"), pointClamp);
		mGDepth = Texture::create(mDevice, frameGraph.getImage("gbuffer_depth"), depthCompare);
		mGBufferFb = Framebuffer::create(mDevice, mWidth, mHeight, true);
		mGBufferFb->addColorAttachment(mGColor->getImage());
		mGBufferFb->addColorAttachment(mGNormal->getImage());
		mGBufferFb->addColorAttachment(mGMR->getImage());
		mGBufferFb->addDepthAttachment(mGDepth->getImage());

		auto createPrev = [&](VkFormat format, VkImageAspectFlags aspect, const SamplerDesc& sampler) {
			VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			auto img = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight),
				format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, VK_SAMPLE_COUNT_1_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);
			return Texture::create(mDevice, img, sampler);
			};
		mPrevGNormal = createPrev(frameGraph.getAttachmentFormat("gbuffer_normal"), VK_IMAGE_ASPECT_COLOR_BIT, pointClamp);
		mPrevGDepth = createPrev(frameGraph.getAttachmentFormat("gbuffer_depth"), VK_IMAGE_ASPECT_DEPTH_BIT, depthCompare);
		
		auto cmd = CommandBuffer::create(mDevice, commandPool);
		cmd->begin();
		auto initImageLayout = [&](const Image::Ptr& img) {
			cmd->transitionImageLayout(img->getImage(), img->getFormat(),
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			};
		initImageLayout(mPrevGNormal->getImage());
		initImageLayout(mPrevGDepth->getImage());
		cmd->end();
		cmd->submitSync(mDevice->getGraphicQueue());
	}

	void GeometryPass::createPipeline(const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
									  const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout) {
		
		mPipeline = Pipeline::create(mDevice);

		// attachment
		mPipeline->setColorAttachmentFormats({ frameGraph.getAttachmentFormat("gbuffer_color"), frameGraph.getAttachmentFormat("gbuffer_normal"),
											   frameGraph.getAttachmentFormat("gbuffer_metallic_roughness")});
		VkFormat depthFormat = frameGraph.getAttachmentFormat("gbuffer_depth");;
		mPipeline->setDepthAttachmentFormat(depthFormat);
		if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
			mPipeline->setStencilAttachmentFormat(depthFormat);
		}

		// viewport
		std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		mPipeline->setDynamicStates(dynamicStates);

		// shader
		std::vector<Shader::Ptr> shaderGroup{};
		shaderGroup.push_back(Shader::create(mDevice, "shaders/gbuffer/gbuffer_vs.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"));
		shaderGroup.push_back(Shader::create(mDevice, "shaders/gbuffer/gbuffer_fs.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"));
		mPipeline->setShaderGroup(shaderGroup);

		// vertex
		auto vertexBindingDes = geometryBuffer->getVertexInputBindingDescriptions();
		auto attributeDes = geometryBuffer->getAttributeDescriptions();
		mPipeline->mVertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDes.size());
		mPipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
		mPipeline->mVertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDes.size());
		mPipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

		mPipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mPipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		mPipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		// rasterization
		mPipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mPipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;
		mPipeline->mRasterState.lineWidth = 1.0f;
		mPipeline->mRasterState.cullMode = VK_CULL_MODE_BACK_BIT;
		mPipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		mPipeline->mRasterState.depthBiasEnable = VK_FALSE;
		mPipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		mPipeline->mRasterState.depthBiasClamp = 0.0f;
		mPipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		// MSAA
		mPipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		mPipeline->mSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		mPipeline->mSampleState.minSampleShading = 1.0f;
		mPipeline->mSampleState.pSampleMask = nullptr;
		mPipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		mPipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		// depth stencil
		mPipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		mPipeline->mDepthStencilState.depthWriteEnable =VK_TRUE;
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
		for (uint32_t i = 0; i < 3; ++i) {
			mPipeline->pushBlendAttachments(blendAttachment);
		}

		mPipeline->mBlendState.logicOpEnable = VK_FALSE;
		mPipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;
		mPipeline->mBlendState.blendConstants[0] = 0.0f;
		mPipeline->mBlendState.blendConstants[1] = 0.0f;
		mPipeline->mBlendState.blendConstants[2] = 0.0f;
		mPipeline->mBlendState.blendConstants[3] = 0.0f;

		mPipeline->mSetLayoutsStorage = { frameSetLayout->getLayout(), staticSetLayout->getLayout()};
		mPipeline->mLayoutState.setLayoutCount = static_cast<uint32_t>(mPipeline->mSetLayoutsStorage.size());
		mPipeline->mLayoutState.pSetLayouts = mPipeline->mSetLayoutsStorage.data();
		mPipeline->mLayoutState.flags = 0;
		mPipeline->mLayoutState.pushConstantRangeCount = 0;
		mPipeline->mLayoutState.pPushConstantRanges = nullptr;
		
		mPipeline->build();
	}


	void GeometryPass::render(const CommandBuffer::Ptr& cmd, const GeometryBuffer::Ptr& geoBuffer,
							  const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet,
							  Camera camera, DirectionalLight light, int frameIndex) {

		std::vector<Image::Ptr> colorImages = mGBufferFb->getColorAttachments();
		for (auto& image : colorImages) {
			cmd->transitionImageLayout(image->getImage(), image->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		}
		
		cmd->transitionImageLayout(mGBufferFb->getDepthAttachment()->getImage(), mGBufferFb->getDepthAttachment()->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

		cmd->beginRendering(mGBufferFb);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(mWidth);
		viewport.height = static_cast<float>(mHeight);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd->setViewport(0, viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { mWidth, mHeight };
		cmd->setScissor(0, scissor);

		cmd->bindGraphicPipeline(mPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getLayout(), staticSet->getDescriptorSet(0), 1);
		geoBuffer->draw(cmd);

		cmd->endRendering();

		for (auto& image : colorImages) {
			cmd->transitionImageLayout(image->getImage(), image->getFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		cmd->transitionImageLayout(mGBufferFb->getDepthAttachment()->getImage(), mGBufferFb->getDepthAttachment()->getFormat(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}
}



