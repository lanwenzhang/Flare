#include "taa_pass.h"

namespace flare::renderer {

    void TAAPass::init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const Texture::Ptr& depthTex, const Texture::Ptr& lightingTex,
                       const Texture::Ptr& motionVectorTex, const DescriptorSetLayout::Ptr& frameSetLayout,int frameCount) {

        mPreviousHistoryIndex = 0;
        mCurrentHistoryIndex = 0;
        createImages(commandPool, frameGraph);
        createDescriptor(depthTex, lightingTex, motionVectorTex, frameCount);
        createPipeline(frameSetLayout);
    }

    void TAAPass::createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph) {
        
        SamplerDesc pointClamp{};
        pointClamp.minFilter = VK_FILTER_NEAREST;
        pointClamp.magFilter = VK_FILTER_NEAREST;
        pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        pointClamp.anisotropyEnable = VK_FALSE;
        pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        SamplerDesc linearClamp = pointClamp;
        linearClamp.minFilter = VK_FILTER_LINEAR;
        linearClamp.magFilter = VK_FILTER_LINEAR;

        VkFormat taaFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkImageUsageFlags taaUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        auto taa0 = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight), taaFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            taaUsage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        auto taa1 = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight), taaFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            taaUsage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        mHistory0 = Texture::create(mDevice, taa0, linearClamp);
        mHistory1 = Texture::create(mDevice, taa1, linearClamp);

        auto cmd = CommandBuffer::create(mDevice, commandPool);
        cmd->begin();
        auto initImageLayout = [&](const Image::Ptr& img) {
            cmd->transitionImageLayout(img->getImage(), img->getFormat(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            };
        initImageLayout(mHistory0->getImage());
        initImageLayout(mHistory1->getImage());
        cmd->end();
        cmd->submitSync(mDevice->getGraphicQueue());
    }

    void TAAPass::createDescriptor(const Texture::Ptr& depthTex, const Texture::Ptr& lightingTex, const Texture::Ptr& motionVectorTex,int frameCount) {

        std::vector<UniformParameter::Ptr> params;
        mDepthParam = UniformParameter::create();
        mDepthParam->mBinding = 0;
        mDepthParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mDepthParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mDepthParam->mCount = 1;
        mDepthParam->mImageInfos.resize(1);
        mDepthParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mDepthParam->mImageInfos[0].imageView = depthTex->getImageView();
        mDepthParam->mImageInfos[0].sampler = depthTex->getSampler();
        params.push_back(mDepthParam);

        mMotionVectorParam = UniformParameter::create();
        mMotionVectorParam->mBinding = 1;
        mMotionVectorParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mMotionVectorParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mMotionVectorParam->mCount = 1;
        mMotionVectorParam->mImageInfos.resize(1);
        mMotionVectorParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mMotionVectorParam->mImageInfos[0].imageView = motionVectorTex->getImageView();
        mMotionVectorParam->mImageInfos[0].sampler = motionVectorTex->getSampler();
        params.push_back(mMotionVectorParam);

        mCurrentColorParam = UniformParameter::create();
        mCurrentColorParam->mBinding = 2;
        mCurrentColorParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mCurrentColorParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mCurrentColorParam->mCount = 1;
        mCurrentColorParam->mImageInfos.resize(1);
        mCurrentColorParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mCurrentColorParam->mImageInfos[0].imageView = lightingTex->getImageView();
        mCurrentColorParam->mImageInfos[0].sampler = lightingTex->getSampler();
        params.push_back(mCurrentColorParam);

        mHistoryReadParam = UniformParameter::create();
        mHistoryReadParam->mBinding = 3;
        mHistoryReadParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mHistoryReadParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mHistoryReadParam->mCount = 1;
        mHistoryReadParam->mImageInfos.resize(1);
        mHistoryReadParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mHistoryReadParam->mImageInfos[0].imageView = mHistory0->getImageView();
        mHistoryReadParam->mImageInfos[0].sampler = mHistory0->getSampler();
        params.push_back(mHistoryReadParam);

        mHistoryWriteParam = UniformParameter::create();
        mHistoryWriteParam->mBinding = 4;
        mHistoryWriteParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        mHistoryWriteParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mHistoryWriteParam->mCount = 1;
        mHistoryWriteParam->mImageInfos.resize(1);
        mHistoryWriteParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        mHistoryWriteParam->mImageInfos[0].imageView = mHistory1->getImageView();
        mHistoryWriteParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
        params.push_back(mHistoryWriteParam);


        mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
        mDescriptorSetLayout->build(params);

        mDescriptorPool = DescriptorPool::create(mDevice);
        mDescriptorPool->build(params, frameCount);

        mDescriptorSet = DescriptorSet::create(mDevice, params, mDescriptorSetLayout, mDescriptorPool, frameCount);

        for (int i = 0; i < frameCount; ++i) {
            auto vkSet = mDescriptorSet->getDescriptorSet(i);
            mDescriptorSet->updateImage(vkSet, mDepthParam->mBinding, mDepthParam->mImageInfos[0]);
            mDescriptorSet->updateImage(vkSet, mMotionVectorParam->mBinding, mMotionVectorParam->mImageInfos[0]);
            mDescriptorSet->updateImage(vkSet, mCurrentColorParam->mBinding, mCurrentColorParam->mImageInfos[0]);
            mDescriptorSet->updateImage(vkSet, mHistoryReadParam->mBinding, mHistoryReadParam->mImageInfos[0]);
            mDescriptorSet->updateStorageImage(vkSet, mHistoryWriteParam->mBinding, mHistoryWriteParam->mImageInfos[0]);
        }
    }

    void TAAPass::createPipeline(const DescriptorSetLayout::Ptr& frameSetLayout) {
        mPipeline = ComputePipeline::create(mDevice);
        auto taaShader = Shader::create(mDevice, "shaders/taa/taa_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
        mPipeline->setShader(taaShader);
        mPipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), mDescriptorSetLayout->getLayout() });
        mPipeline->build();
    }

    void TAAPass::render(const CommandBuffer::Ptr& cmd, const Texture::Ptr& lightingTex, const DescriptorSet::Ptr& frameSet, int frameIndex) {

        mPreviousHistoryIndex = mCurrentHistoryIndex;
        mCurrentHistoryIndex = (mCurrentHistoryIndex + 1) % 2;

        Texture::Ptr historyReadTex = (mPreviousHistoryIndex == 0) ? mHistory0 : mHistory1;
        Texture::Ptr historyWriteTex = (mCurrentHistoryIndex == 0) ? mHistory0 : mHistory1;

        cmd->transitionImageLayout(historyWriteTex->getImage()->getImage(), historyWriteTex->getImage()->getFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        VkDescriptorImageInfo historyReadInfo{};
        historyReadInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        historyReadInfo.imageView = historyReadTex->getImageView();
        historyReadInfo.sampler = historyReadTex->getSampler();

        VkDescriptorImageInfo historyWriteInfo{};
        historyWriteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        historyWriteInfo.imageView = historyWriteTex->getImageView();
        historyWriteInfo.sampler = VK_NULL_HANDLE;

        auto ds = mDescriptorSet->getDescriptorSet(frameIndex);
        mDescriptorSet->updateImage(ds, mHistoryReadParam->mBinding, historyReadInfo);
        mDescriptorSet->updateStorageImage(ds, mHistoryWriteParam->mBinding, historyWriteInfo);

        cmd->bindComputePipeline(mPipeline->getPipeline());
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 1);

        const uint32_t groupX = (mWidth + 7) / 8;
        const uint32_t groupY = (mHeight + 7) / 8;
        cmd->dispatch(groupX, groupY, 1);

        cmd->transitionImageLayout(historyWriteTex->getImage()->getImage(), historyWriteTex->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

}