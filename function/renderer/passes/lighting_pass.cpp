#include "lighting_pass.h"

namespace flare::renderer {

	void LightingPass::init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const DescriptorSetLayout::Ptr& frameSetLayout, 
							const GeometryPass::Ptr& geometryPass, const DDGIPass::Ptr& ddgiPass,
							const Texture::Ptr& visibilityTex, const Texture::Ptr& indirectTex, const GTAOPass::Ptr& gtaoPass,
							const CubeMapTexture::Ptr& skybox, const SphereBuffer::Ptr& sphereBuffer, int frameCount) {
		mColor = geometryPass->getGbufferColor();
		mNormal = geometryPass->getGbufferNormal();
		mDepth = geometryPass->getGbufferDepth();
		mMR = geometryPass->getGbufferMR();
		mVisibility = visibilityTex;
		mSkybox = skybox;

		mIndirect = indirectTex;
		mProbeIrradiance = ddgiPass->getProbeIrradiance();
		mProbeOffset = ddgiPass->getPrevOffset();

		mAORaw = gtaoPass->getAO();
	
		mFrameCount = frameCount;
		createImages(commandPool, frameGraph, geometryPass);
		createDescriptorSet();
		createSphereDS();
		createPipeline(frameGraph, frameSetLayout, ddgiPass->getProbeRTDSLayout(), sphereBuffer);
	}

	void LightingPass::createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const GeometryPass::Ptr& geometryPass){
		
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

		SamplerDesc linearClamp = pointClamp;
		linearClamp.minFilter = VK_FILTER_LINEAR;
		linearClamp.magFilter = VK_FILTER_LINEAR;
		
		mIrradianceMap = CubeMapTexture::create(mDevice, commandPool, "assets/skybox/immenstadter_horn_2k_irradiance.ktx");
		auto cmd2 = CommandBuffer::create(mDevice, commandPool);
		cmd2->begin();
		cmd2->transitionImageLayout(mIrradianceMap->getImage()->getImage(), mIrradianceMap->getImage()->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, 0, 6);
		cmd2->end();
		cmd2->submitSync(mDevice->getGraphicQueue());

		mLighting = Texture::create(mDevice, frameGraph.getImage("lighting"), linearClamp);
		mLightingFb = Framebuffer::create(mDevice, mWidth, mHeight, true);
		mLightingFb->addColorAttachment(mLighting->getImage());
		mLightingFb->addDepthAttachment(geometryPass->getGbufferDepth()->getImage());
	}

	void LightingPass::createDescriptorSet() {
		auto addParam = [&](uint32_t binding, const Texture::Ptr& tex) {
			auto param = UniformParameter::create();
			param->mBinding = binding;
			param->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			param->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
			param->mCount = 1;
			param->mImageInfos.resize(1);
			param->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			param->mImageInfos[0].imageView = tex->getImageView();
			param->mImageInfos[0].sampler = tex->getSampler();
			mParams.push_back(param);

		};
		addParam(0, mColor);
		addParam(1, mNormal);
		addParam(2, mMR);
		addParam(3, mDepth);

		auto param = UniformParameter::create();
		param->mBinding = 4;
		param->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param->mCount = 1;
		param->mImageInfos.resize(1);
		param->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param->mImageInfos[0].imageView = mIrradianceMap->getImageView();
		param->mImageInfos[0].sampler = mIrradianceMap->getSampler();
		mParams.push_back(param);

		auto param2 = UniformParameter::create();
		param2->mBinding = 5;
		param2->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param2->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param2->mCount = 1;
		param2->mImageInfos.resize(1);
		param2->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param2->mImageInfos[0].imageView = mVisibility->getImageView();
		param2->mImageInfos[0].sampler = mVisibility->getSampler();
		mParams.push_back(param2);

		auto param3 = UniformParameter::create();
		param3->mBinding = 6;
		param3->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param3->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param3->mCount = 1;
		param3->mImageInfos.resize(1);
		param3->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param3->mImageInfos[0].imageView = mSkybox->getImageView();
		param3->mImageInfos[0].sampler = mSkybox->getSampler();
		mParams.push_back(param3);

		auto param4 = UniformParameter::create();
		param4->mBinding = 7;
		param4->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param4->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param4->mCount = 1;
		param4->mImageInfos.resize(1);
		param4->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param4->mImageInfos[0].imageView = mIndirect->getImageView();
		param4->mImageInfos[0].sampler = mIndirect->getSampler();
		mParams.push_back(param4);

		auto param5 = UniformParameter::create();
		param5->mBinding = 8;
		param5->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param5->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param5->mCount = 1;
		param5->mImageInfos.resize(1);
		param5->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param5->mImageInfos[0].imageView = mAORaw->getImageView();
		param5->mImageInfos[0].sampler = mAORaw->getSampler();
		mParams.push_back(param5);

		mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout->build(mParams);
		mDescriptorPool = DescriptorPool::create(mDevice);
		mDescriptorPool->build(mParams, mFrameCount);
		mDescriptorSet = DescriptorSet::create(mDevice, mParams, mDescriptorSetLayout, mDescriptorPool, mFrameCount);

		for (int i = 0; i < mFrameCount; ++i) {
			for (auto p : mParams) {
				mDescriptorSet->updateImage(mDescriptorSet->getDescriptorSet(i), p->mBinding, p->mImageInfos[0]);
			}
		}
	}

	void LightingPass::createSphereDS() {
		mSphereParams.clear();

		auto param = UniformParameter::create();
		param->mBinding = 0;
		param->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		param->mCount = 1;
		param->mImageInfos.resize(1);
		param->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param->mImageInfos[0].imageView = mProbeIrradiance->getImageView();
		param->mImageInfos[0].sampler = mProbeIrradiance->getSampler();
		mSphereParams.push_back(param);

		auto param1 = UniformParameter::create();
		param1->mBinding = 1;
		param1->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		param1->mStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		param1->mCount = 1;
		param1->mImageInfos.resize(1);
		param1->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		param1->mImageInfos[0].imageView = mProbeOffset->getImageView();
		param1->mImageInfos[0].sampler = mProbeOffset->getSampler();
		mSphereParams.push_back(param1);

		mSphereDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
		mSphereDescriptorSetLayout->build(mSphereParams);
		mSphereDescriptorPool = DescriptorPool::create(mDevice);
		mSphereDescriptorPool->build(mSphereParams, mFrameCount);
		mSphereDescriptorSet = DescriptorSet::create(mDevice, mSphereParams, mSphereDescriptorSetLayout, mSphereDescriptorPool, mFrameCount);

		for (int i = 0; i < mFrameCount; ++i) {
			for (auto p : mSphereParams) {
				mSphereDescriptorSet->updateImage(mSphereDescriptorSet->getDescriptorSet(i), p->mBinding, p->mImageInfos[0]);
			}
		}
	}

	void LightingPass::createPipeline(const FrameGraph& frameGraph, const DescriptorSetLayout::Ptr& frameSetLayout, 
									  const DescriptorSetLayout::Ptr& probeSetLayout, const SphereBuffer::Ptr& sphereBuffer) {
		// 1 scene
		mPipeline = Pipeline::create(mDevice);
		
		// attachment
		mPipeline->setColorAttachmentFormats({ frameGraph.getAttachmentFormat("lighting") });
		VkFormat depthFormat = frameGraph.getAttachmentFormat("gbuffer_depth");
		mPipeline->setDepthAttachmentFormat(depthFormat);
		if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
			mPipeline->setStencilAttachmentFormat(depthFormat);
		}

		// viewport
		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		mPipeline->setDynamicStates(dynamicStates);

		// shader
		std::vector<Shader::Ptr> shaderGroup{};
		shaderGroup.push_back(Shader::create(mDevice, "shaders/tone_mapping/quad_flip_vs.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"));
		shaderGroup.push_back(Shader::create(mDevice, "shaders/lighting/lighting_fs.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"));
		mPipeline->setShaderGroup(shaderGroup);

		// vertex
		mPipeline->mVertexInputState.vertexBindingDescriptionCount = 0;
		mPipeline->mVertexInputState.pVertexBindingDescriptions = nullptr;
		mPipeline->mVertexInputState.vertexAttributeDescriptionCount = 0;
		mPipeline->mVertexInputState.pVertexAttributeDescriptions = nullptr;

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

		// MSAA
		mPipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		mPipeline->mSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		mPipeline->mSampleState.minSampleShading = 1.0f;
		mPipeline->mSampleState.pSampleMask = nullptr;
		mPipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		mPipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		// depth stencil
		mPipeline->mDepthStencilState.depthTestEnable = VK_FALSE;
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

		mPipeline->mSetLayoutsStorage = { frameSetLayout->getLayout(), mDescriptorSetLayout->getLayout() };
		mPipeline->mLayoutState.setLayoutCount = static_cast<uint32_t>(mPipeline->mSetLayoutsStorage.size());
		mPipeline->mLayoutState.pSetLayouts = mPipeline->mSetLayoutsStorage.data();
		mPipeline->mLayoutState.flags = 0;

		VkPushConstantRange lightPcRange{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(flare::app::LightPushConstant) };
		mPipeline->mPushConstantRanges = { lightPcRange };
		mPipeline->mLayoutState.pushConstantRangeCount = static_cast<uint32_t>(mPipeline->mPushConstantRanges.size());
		mPipeline->mLayoutState.pPushConstantRanges = mPipeline->mPushConstantRanges.data();

		mPipeline->build();

		// 2 skybox
		mSkyboxPipeline = Pipeline::create(mDevice);
		mSkyboxPipeline->setColorAttachmentFormats({ frameGraph.getAttachmentFormat("lighting") });
		mSkyboxPipeline->setDepthAttachmentFormat(depthFormat);
		mSkyboxPipeline->setDynamicStates(dynamicStates);

		std::vector<Shader::Ptr> skyboxShaders{};
		skyboxShaders.push_back(Shader::create(mDevice, "shaders/skybox/skybox_vs.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"));
		skyboxShaders.push_back(Shader::create(mDevice, "shaders/skybox/skybox_fs.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"));
		mSkyboxPipeline->setShaderGroup(skyboxShaders);

		mSkyboxPipeline->mVertexInputState.vertexBindingDescriptionCount = 0;
		mSkyboxPipeline->mVertexInputState.pVertexBindingDescriptions = nullptr;
		mSkyboxPipeline->mVertexInputState.vertexAttributeDescriptionCount = 0;
		mSkyboxPipeline->mVertexInputState.pVertexAttributeDescriptions = nullptr;

		mSkyboxPipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mSkyboxPipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		mSkyboxPipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		mSkyboxPipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mSkyboxPipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;
		mSkyboxPipeline->mRasterState.lineWidth = 1.0f;
		mSkyboxPipeline->mRasterState.cullMode = VK_CULL_MODE_NONE;
		mSkyboxPipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		mSkyboxPipeline->mRasterState.depthBiasEnable = VK_FALSE;
		mSkyboxPipeline->mRasterState.depthBiasConstantFactor = 0.0f;
		mSkyboxPipeline->mRasterState.depthBiasClamp = 0.0f;
		mSkyboxPipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

		mSkyboxPipeline->mSampleState.sampleShadingEnable = VK_FALSE;
		mSkyboxPipeline->mSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		mSkyboxPipeline->mSampleState.minSampleShading = 1.0f;
		mSkyboxPipeline->mSampleState.pSampleMask = nullptr;
		mSkyboxPipeline->mSampleState.alphaToCoverageEnable = VK_FALSE;
		mSkyboxPipeline->mSampleState.alphaToOneEnable = VK_FALSE;

		mSkyboxPipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		mSkyboxPipeline->mDepthStencilState.depthWriteEnable = VK_FALSE;
		mSkyboxPipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		VkPipelineColorBlendAttachmentState skyboxBlendAttachment{};
		skyboxBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		skyboxBlendAttachment.blendEnable = VK_FALSE;
		skyboxBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		skyboxBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		skyboxBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		skyboxBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		skyboxBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		skyboxBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		mSkyboxPipeline->pushBlendAttachments(skyboxBlendAttachment);

		mSkyboxPipeline->mBlendState.logicOpEnable = VK_FALSE;
		mSkyboxPipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;
		mSkyboxPipeline->mBlendState.blendConstants[0] = 0.0f;
		mSkyboxPipeline->mBlendState.blendConstants[1] = 0.0f;
		mSkyboxPipeline->mBlendState.blendConstants[2] = 0.0f;
		mSkyboxPipeline->mBlendState.blendConstants[3] = 0.0f;

		mSkyboxPipeline->mSetLayoutsStorage = { frameSetLayout->getLayout(), mDescriptorSetLayout->getLayout() };
		mSkyboxPipeline->mLayoutState.setLayoutCount = static_cast<uint32_t>(mSkyboxPipeline->mSetLayoutsStorage.size());
		mSkyboxPipeline->mLayoutState.pSetLayouts = mSkyboxPipeline->mSetLayoutsStorage.data();
		mSkyboxPipeline->mLayoutState.flags = 0;
		mSkyboxPipeline->mLayoutState.pushConstantRangeCount = 0;
		mSkyboxPipeline->mLayoutState.pPushConstantRanges = nullptr;

		mSkyboxPipeline->build();

		// 3 ddgi probe visualization
		mSpherePipeline = Pipeline::create(mDevice);
		mSpherePipeline->setColorAttachmentFormats({ frameGraph.getAttachmentFormat("lighting") });
		mSpherePipeline->setDepthAttachmentFormat(depthFormat);
		mSpherePipeline->setDynamicStates(dynamicStates);

		std::vector<Shader::Ptr> sphereShaders{};
		sphereShaders.push_back(Shader::create(mDevice, "shaders/debug/probe_visualization_vs.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"));
		sphereShaders.push_back(Shader::create(mDevice, "shaders/debug/probe_visualization_fs.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"));
		mSpherePipeline->setShaderGroup(sphereShaders);

		auto vertexBindingDes = sphereBuffer->getVertexInputBindingDescriptions();
		auto attributeDes = sphereBuffer->getAttributeDescriptions();
		mSpherePipeline->mVertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDes.size());
		mSpherePipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
		mSpherePipeline->mVertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDes.size());
		mSpherePipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

		mSpherePipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mSpherePipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		mSpherePipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

		mSpherePipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mSpherePipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;
		mSpherePipeline->mRasterState.lineWidth = 1.0f;
		mSpherePipeline->mRasterState.cullMode = VK_CULL_MODE_BACK_BIT;
		mSpherePipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		mSpherePipeline->mRasterState.depthBiasEnable = VK_FALSE;

		mSpherePipeline->mSampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		mSpherePipeline->mSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		mSpherePipeline->mSampleState.sampleShadingEnable = VK_FALSE;

		mSpherePipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
		mSpherePipeline->mDepthStencilState.depthWriteEnable = VK_TRUE;
		mSpherePipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		VkPipelineColorBlendAttachmentState probeBlendAttachment{};
		probeBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		probeBlendAttachment.blendEnable = VK_FALSE;
		probeBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		probeBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		probeBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		probeBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		probeBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		probeBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		mSpherePipeline->pushBlendAttachments(probeBlendAttachment);
		
		mSpherePipeline->mBlendState.logicOpEnable = VK_FALSE;
		mSpherePipeline->mBlendState.logicOp = VK_LOGIC_OP_COPY;
		mSpherePipeline->mBlendState.blendConstants[0] = 0.0f;
		mSpherePipeline->mBlendState.blendConstants[1] = 0.0f;
		mSpherePipeline->mBlendState.blendConstants[2] = 0.0f;
		mSpherePipeline->mBlendState.blendConstants[3] = 0.0f;

		mSpherePipeline->mSetLayoutsStorage = { frameSetLayout->getLayout(), probeSetLayout->getLayout(), mSphereDescriptorSetLayout->getLayout()};
		mSpherePipeline->mLayoutState.setLayoutCount = static_cast<uint32_t>(mSpherePipeline->mSetLayoutsStorage.size());
		mSpherePipeline->mLayoutState.pSetLayouts = mSpherePipeline->mSetLayoutsStorage.data();
		mSpherePipeline->mLayoutState.flags = 0;
		
		VkPushConstantRange camPosRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4) };
		mSpherePipeline->mPushConstantRanges = { camPosRange };
		mSpherePipeline->mLayoutState.pushConstantRangeCount = static_cast<uint32_t>(mSpherePipeline->mPushConstantRanges.size());
		mSpherePipeline->mLayoutState.pPushConstantRanges = mSpherePipeline->mPushConstantRanges.data();

		mSpherePipeline->build();
	}

	void LightingPass::setShadowTexture(const Texture::Ptr& tex, int frameIndex) {
		mShadowTex = tex;
		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = tex->getImageView();
		info.sampler = tex->getSampler();
		mDescriptorSet->updateImage(mDescriptorSet->getDescriptorSet(frameIndex), 5, info);
	}

	void LightingPass::setAOTexture(const Texture::Ptr& tex, int frameIndex) {
		
		mAOTex = tex;
		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = tex->getImageView();
		info.sampler = tex->getSampler();
		mDescriptorSet->updateImage(mDescriptorSet->getDescriptorSet(frameIndex), 8, info);
	}

	void LightingPass::setProbeIrradianceTexture(const Texture::Ptr& tex, int frameIndex) {
		
		mProbeIrradiance = tex;
		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = mProbeIrradiance->getImageView();
		info.sampler = mProbeIrradiance->getSampler();
		mSphereDescriptorSet->updateImage(mSphereDescriptorSet->getDescriptorSet(frameIndex), 0, info);
	}

	void LightingPass::render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& probeSet,
							  const SphereBuffer::Ptr& sphereBuffer, int probeCount,
							  Camera camera, DirectionalLight light, flare::app::LightPushConstant pc, int frameIndex) {
		
		std::vector<Image::Ptr> colorImages = mLightingFb->getColorAttachments();
		cmd->transitionImageLayout(colorImages[0]->getImage(), colorImages[0]->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		cmd->beginRendering(mLightingFb, VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_LOAD_OP_LOAD);

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
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 1);
		pc.cameraPos = glm::vec4(camera.getPosition(), 0.0f);
		cmd->pushConstants(mPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, pc);
		cmd->draw(3);

		if (mShowProbe) {
			cmd->bindGraphicPipeline(mSpherePipeline->getPipeline());
			cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mSpherePipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
			cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mSpherePipeline->getLayout(), probeSet->getDescriptorSet(frameIndex), 1);
			cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mSpherePipeline->getLayout(), mSphereDescriptorSet->getDescriptorSet(frameIndex), 2);
			cmd->pushConstants(mSpherePipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT, pc.cameraPos);
			cmd->bindVertexBuffer({ sphereBuffer->getVertexBuffer()->getBuffer() });
			cmd->bindIndexBuffer(sphereBuffer->getIndexBuffer()->getBuffer());
			cmd->drawIndexInstanced(sphereBuffer->getIndexCount(), probeCount);
		}

		cmd->bindGraphicPipeline(mSkyboxPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mSkyboxPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, mSkyboxPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 1);
		cmd->draw(36);
		cmd->endRendering();

		cmd->transitionImageLayout(
			mAORaw->getImage()->getImage(),
			mAORaw->getImage()->getFormat(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		cmd->transitionImageLayout(colorImages[0]->getImage(), colorImages[0]->getFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	}
}