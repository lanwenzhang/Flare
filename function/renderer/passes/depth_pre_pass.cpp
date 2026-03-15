#include "depth_pre_pass.h"

namespace flare::renderer {

    void DepthPrePass::init(const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
                            const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout) {
        createImages(frameGraph);
        createPipeline(frameGraph, geometryBuffer, frameSetLayout, staticSetLayout);
    }

    void DepthPrePass::createImages(const FrameGraph& frameGraph) {

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

        mDepthPreLinear = Texture::create(mDevice, frameGraph.getImage("depth_linear"), pointClamp);
        mDepthPre = Texture::create(mDevice, frameGraph.getImage("depth"), depthCompare);
        mDepthPreFb = Framebuffer::create(mDevice, mWidth, mHeight, true);
        mDepthPreFb->addColorAttachment(mDepthPreLinear->getImage());
        mDepthPreFb->addDepthAttachment(mDepthPre->getImage());
    }

    void DepthPrePass::createPipeline(const FrameGraph& frameGraph, const GeometryBuffer::Ptr& geometryBuffer,
        const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout) {
        mPipeline = Pipeline::create(mDevice);

        mPipeline->setColorAttachmentFormats({ frameGraph.getAttachmentFormat("depth_linear") });

        VkFormat depthFormat = frameGraph.getAttachmentFormat("depth");
        mPipeline->setDepthAttachmentFormat(depthFormat);
        if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
            mPipeline->setStencilAttachmentFormat(depthFormat);
        }

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        mPipeline->setDynamicStates(dynamicStates);

        std::vector<Shader::Ptr> shaderGroup{};
        shaderGroup.push_back(Shader::create(mDevice, "shaders/depth_pre/depth_pre_vs.spv", VK_SHADER_STAGE_VERTEX_BIT, "main"));
        shaderGroup.push_back(Shader::create(mDevice, "shaders/depth_pre/depth_pre_fs.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main"));
        mPipeline->setShaderGroup(shaderGroup);

        auto vertexBindingDes = geometryBuffer->getVertexInputBindingDescriptions();
        auto attributeDes = geometryBuffer->getAttributeDescriptions();
        mPipeline->mVertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDes.size());
        mPipeline->mVertexInputState.pVertexBindingDescriptions = vertexBindingDes.data();
        mPipeline->mVertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDes.size());
        mPipeline->mVertexInputState.pVertexAttributeDescriptions = attributeDes.data();

        mPipeline->mAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        mPipeline->mAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        mPipeline->mAssemblyState.primitiveRestartEnable = VK_FALSE;

        mPipeline->mRasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        mPipeline->mRasterState.polygonMode = VK_POLYGON_MODE_FILL;
        mPipeline->mRasterState.lineWidth = 1.0f;
        mPipeline->mRasterState.cullMode = VK_CULL_MODE_BACK_BIT;
        mPipeline->mRasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        mPipeline->mRasterState.depthBiasEnable = VK_FALSE;
        mPipeline->mRasterState.depthBiasConstantFactor = 0.0f;
        mPipeline->mRasterState.depthBiasClamp = 0.0f;
        mPipeline->mRasterState.depthBiasSlopeFactor = 0.0f;

        mPipeline->mSampleState.sampleShadingEnable = VK_FALSE;
        mPipeline->mSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        mPipeline->mDepthStencilState.depthTestEnable = VK_TRUE;
        mPipeline->mDepthStencilState.depthWriteEnable = VK_TRUE;
        mPipeline->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; 
        mPipeline->mDepthStencilState.stencilTestEnable = VK_FALSE;

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

        mPipeline->mSetLayoutsStorage = { frameSetLayout->getLayout(), staticSetLayout->getLayout() };
        mPipeline->mLayoutState.setLayoutCount = static_cast<uint32_t>(mPipeline->mSetLayoutsStorage.size());
        mPipeline->mLayoutState.pSetLayouts = mPipeline->mSetLayoutsStorage.data();
        mPipeline->mLayoutState.flags = 0;
        mPipeline->mLayoutState.pushConstantRangeCount = 0;
        mPipeline->mLayoutState.pPushConstantRanges = nullptr;

        mPipeline->build();
    }

    void DepthPrePass::render(const CommandBuffer::Ptr& cmd, const GeometryBuffer::Ptr& geoBuffer,
                              const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet,
                              Camera camera, int frameIndex) {
        auto colorImg = mDepthPreFb->getColorAttachments();
        auto depthImg = mDepthPreFb->getDepthAttachment();
        
        cmd->transitionImageLayout(colorImg[0]->getImage(), colorImg[0]->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        cmd->transitionImageLayout(depthImg->getImage(), depthImg->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

        cmd->beginRendering(mDepthPreFb);

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

        cmd->transitionImageLayout(colorImg[0]->getImage(), colorImg[0]->getFormat(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        cmd->transitionImageLayout(depthImg->getImage(), depthImg->getFormat(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}

