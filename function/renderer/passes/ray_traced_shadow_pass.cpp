#include "ray_traced_shadow_pass.h"

namespace flare::renderer {

    RayTracedShadowPass::~RayTracedShadowPass() {}

    void RayTracedShadowPass::init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const GeometryPass::Ptr& geometryPass,
                                   const MotionVectorPass::Ptr& motionVectorPass,
                                   const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout, int frameCount) {

        mPrevNormal = geometryPass->getPrevGbufferNormal();
        mPrevDepth = geometryPass->getPrevGbufferDepth();
        mNormal = geometryPass->getGbufferNormal();
        mDepth = geometryPass->getGbufferDepth();
        mMotionVector = motionVectorPass->getMotionVectors();
        mFrameCount = frameCount;

        createImages(commandPool, frameGraph);
        createShadowDS();
        createTemporalFilterDS();
        createAtrousDS();
        createPipeline(frameSetLayout, staticSetLayout);
    }

    void RayTracedShadowPass::createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph) {
        // 1 rt
        SamplerDesc blueNoiseSampler{};
        blueNoiseSampler.minFilter = VK_FILTER_NEAREST;
        blueNoiseSampler.magFilter = VK_FILTER_NEAREST;
        blueNoiseSampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        blueNoiseSampler.anisotropyEnable = VK_FALSE;
        blueNoiseSampler.addressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        blueNoiseSampler.addressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        blueNoiseSampler.addressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        std::string kBlueNoiseSobolTexture = "assets/blue_noise/sobol_256_4d.png";
        std::string kBlueNoiseScramblingRankingTextures[9] = {
            "assets/blue_noise/scrambling_ranking_128x128_2d_1spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_2spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_4spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_8spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_16spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_32spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_64spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_128spp.png",
            "assets/blue_noise/scrambling_ranking_128x128_2d_256spp.png"
        };

        mBlueNoiseSobol = Texture::create(mDevice, commandPool, kBlueNoiseSobolTexture, VK_FORMAT_R8G8B8A8_UNORM, blueNoiseSampler);
        for (int i = 0; i < 9; ++i) {
            mBlueNoiseScramblingRanking[i] =
                Texture::create(mDevice, commandPool, kBlueNoiseScramblingRankingTextures[i], VK_FORMAT_R8G8B8A8_UNORM, blueNoiseSampler);
        }

        SamplerDesc pointClamp{};
        pointClamp.minFilter = VK_FILTER_NEAREST;
        pointClamp.magFilter = VK_FILTER_NEAREST;
        pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        pointClamp.anisotropyEnable = VK_FALSE;
        pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        mVisibility = Texture::create(mDevice, frameGraph.getImage("visibility"), pointClamp);
        VkFormat visibilityFormat = VK_FORMAT_R16G16_SFLOAT;
        VkImageUsageFlags prevVisibilityUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        auto historyVisibilityImage = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight),
                                                    visibilityFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, prevVisibilityUsage, VK_SAMPLE_COUNT_1_BIT,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        mHistoryVisibility = Texture::create(mDevice, historyVisibilityImage, pointClamp);

        VkImageUsageFlags visibilityUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        auto visibilityVarianceImage = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight),
                                                     visibilityFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                     visibilityUsage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        
        // 2 temporal
        mVariance = Texture::create(mDevice, visibilityVarianceImage, pointClamp);

        SamplerDesc linearClamp{};
        linearClamp.minFilter = VK_FILTER_LINEAR;
        linearClamp.magFilter = VK_FILTER_LINEAR;
        linearClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        linearClamp.anisotropyEnable = VK_FALSE;
        linearClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        linearClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        linearClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

        auto momentImage0 = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight),
                                         VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                         usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                                         VK_IMAGE_ASPECT_COLOR_BIT);

        auto momentImage1 = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight),
                                         VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                         usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         VK_IMAGE_ASPECT_COLOR_BIT);

        mMoment0 = Texture::create(mDevice, momentImage0, linearClamp);
        mMoment1 = Texture::create(mDevice, momentImage1, linearClamp);


        // 3 atrous
        VkImageUsageFlags atrousUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        auto atrous0 = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight), visibilityFormat, VK_IMAGE_TYPE_2D,
                                     VK_IMAGE_TILING_OPTIMAL, atrousUsage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        auto atrous1 = Image::create(mDevice, static_cast<int>(mWidth), static_cast<int>(mHeight), visibilityFormat, VK_IMAGE_TYPE_2D,
                                     VK_IMAGE_TILING_OPTIMAL, atrousUsage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        mAtrous0 = Texture::create(mDevice, atrous0, pointClamp);
        mAtrous1 = Texture::create(mDevice, atrous1, pointClamp);

        auto cmd = CommandBuffer::create(mDevice, commandPool);
        cmd->begin();
        cmd->transitionImageLayout(mMoment0->getImage()->getImage(), mMoment0->getImage()->getFormat(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        cmd->transitionImageLayout(mMoment1->getImage()->getImage(), mMoment1->getImage()->getFormat(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        auto initImageLayout = [&](const Image::Ptr& img) {
            cmd->transitionImageLayout(img->getImage(), img->getFormat(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            };
        initImageLayout(mHistoryVisibility->getImage());
        initImageLayout(mAtrous0->getImage());
        initImageLayout(mAtrous1->getImage());
        cmd->end();
        cmd->submitSync(mDevice->getGraphicQueue());
    }

    void RayTracedShadowPass::createShadowDS() {
        std::vector<UniformParameter::Ptr> params;

        mNormalParam = UniformParameter::create();
        mNormalParam->mBinding = 0;
        mNormalParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mNormalParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mNormalParam->mCount = 1;
        mNormalParam->mImageInfos.resize(1);
        mNormalParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mNormalParam->mImageInfos[0].imageView = mNormal->getImageView();
        mNormalParam->mImageInfos[0].sampler = mNormal->getSampler();
        params.push_back(mNormalParam);

        mDepthParam = UniformParameter::create();
        mDepthParam->mBinding = 1;
        mDepthParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mDepthParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mDepthParam->mCount = 1;
        mDepthParam->mImageInfos.resize(1);
        mDepthParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mDepthParam->mImageInfos[0].imageView = mDepth->getImageView();
        mDepthParam->mImageInfos[0].sampler = mDepth->getSampler();
        params.push_back(mDepthParam);

        mVisibilityParam = UniformParameter::create();
        mVisibilityParam->mBinding = 2;
        mVisibilityParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        mVisibilityParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mVisibilityParam->mCount = 1;
        mVisibilityParam->mImageInfos.resize(1);
        mVisibilityParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        mVisibilityParam->mImageInfos[0].imageView = mVisibility->getImageView();
        mVisibilityParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
        params.push_back(mVisibilityParam);

        mBlueNoiseSobolParam = UniformParameter::create();
        mBlueNoiseSobolParam->mBinding = 3;
        mBlueNoiseSobolParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mBlueNoiseSobolParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mBlueNoiseSobolParam->mCount = 1;
        mBlueNoiseSobolParam->mImageInfos.resize(1);
        mBlueNoiseSobolParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mBlueNoiseSobolParam->mImageInfos[0].imageView = mBlueNoiseSobol->getImageView();
        mBlueNoiseSobolParam->mImageInfos[0].sampler = mBlueNoiseSobol->getSampler();
        params.push_back(mBlueNoiseSobolParam);

        mBlueNoiseScramblingParam = UniformParameter::create();
        mBlueNoiseScramblingParam->mBinding = 4;
        mBlueNoiseScramblingParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mBlueNoiseScramblingParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mBlueNoiseScramblingParam->mCount = static_cast<uint32_t>(mBlueNoiseScramblingRanking.size());
        mBlueNoiseScramblingParam->mImageInfos.resize(mBlueNoiseScramblingParam->mCount);

        for (uint32_t i = 0; i < mBlueNoiseScramblingParam->mCount; ++i){
            mBlueNoiseScramblingParam->mImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            mBlueNoiseScramblingParam->mImageInfos[i].imageView = mBlueNoiseScramblingRanking[i]->getImageView();
            mBlueNoiseScramblingParam->mImageInfos[i].sampler = mBlueNoiseScramblingRanking[i]->getSampler();
        }
        params.push_back(mBlueNoiseScramblingParam);

        mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
        mDescriptorSetLayout->build(params);
        mDescriptorPool = DescriptorPool::create(mDevice);
        mDescriptorPool->build(params, mFrameCount);
        mDescriptorSet = DescriptorSet::create(mDevice, params, mDescriptorSetLayout, mDescriptorPool, mFrameCount);

        for (int i = 0; i < mFrameCount; ++i) {
            VkDescriptorSet set = mDescriptorSet->getDescriptorSet(i);
            mDescriptorSet->updateImage(set, mNormalParam->mBinding, mNormalParam->mImageInfos[0]);
            mDescriptorSet->updateImage(set, mDepthParam->mBinding, mDepthParam->mImageInfos[0]);
            mDescriptorSet->updateStorageImage(set, mVisibilityParam->mBinding, mVisibilityParam->mImageInfos[0]);
            mDescriptorSet->updateImage(set, mBlueNoiseSobolParam->mBinding, mBlueNoiseSobolParam->mImageInfos[0]);
        }
    }

    void RayTracedShadowPass::createTemporalFilterDS(){
        enum TemporalBinding{
            BINDING_PREV_NORMAL = 0,
            BINDING_PREV_DEPTH = 1,
            BINDING_CURR_NORMAL = 2,
            BINDING_CURR_DEPTH = 3,
            BINDING_MOTION = 4,
            BINDING_HISTORY_VISIBILITY = 5,
            BINDING_VISIBILITY = 6,
            BINDING_MOMENT_READ = 7,
            BINDING_MOMENT_WRITE = 8,
            BINDING_VARIANCE = 9,
        };

        std::vector<UniformParameter::Ptr> params;
        auto makeSamplerParam = [&](UniformParameter::Ptr& outParam, TemporalBinding binding, const Texture::Ptr& tex){
            auto p = UniformParameter::create();
            p->mBinding = static_cast<uint32_t>(binding);
            p->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            p->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
            p->mCount = 1;
            p->mImageInfos.resize(1);
            p->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            p->mImageInfos[0].imageView = tex->getImageView();
            p->mImageInfos[0].sampler = tex->getSampler();

            outParam = p;
            params.push_back(p);
        };

        auto makeStorageParam = [&](UniformParameter::Ptr& outParam, TemporalBinding binding, const Texture::Ptr& tex){
            auto p = UniformParameter::create();
            p->mBinding = static_cast<uint32_t>(binding);
            p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            p->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
            p->mCount = 1;
            p->mImageInfos.resize(1);
            p->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            p->mImageInfos[0].imageView = tex->getImageView();
            p->mImageInfos[0].sampler = VK_NULL_HANDLE;

            outParam = p;
            params.push_back(p);
        };

        makeSamplerParam(mPrevNormalTemporalParam, BINDING_PREV_NORMAL, mPrevNormal);
        makeSamplerParam(mPrevDepthTemporalParam, BINDING_PREV_DEPTH, mPrevDepth);
        makeSamplerParam(mCurrNormalTemporalParam, BINDING_CURR_NORMAL, mNormal);
        makeSamplerParam(mCurrDepthTemporalParam, BINDING_CURR_DEPTH, mDepth);
        makeSamplerParam(mMotionTemporalParam, BINDING_MOTION, mMotionVector);
        makeSamplerParam(mHistoryVisibilityParam, BINDING_HISTORY_VISIBILITY, mHistoryVisibility);
        makeSamplerParam(mVisibilityTemporalParam, BINDING_VISIBILITY, mVisibility);
        Texture::Ptr historyMomentTex = (mCurrentMomentsIndex == 0) ? mMoment0 : mMoment1;
        Texture::Ptr writeMomentTex = (mCurrentMomentsIndex == 0) ? mMoment1 : mMoment0;
        makeSamplerParam(mMomentReadParam, BINDING_MOMENT_READ, historyMomentTex);
        makeStorageParam(mMomentWriteParam, BINDING_MOMENT_WRITE, writeMomentTex);
        makeStorageParam(mVarianceParam, BINDING_VARIANCE, mVariance);

        mTemporalSetLayout = DescriptorSetLayout::create(mDevice);
        mTemporalSetLayout->build(params);
        mTemporalDescriptorPool = DescriptorPool::create(mDevice);
        mTemporalDescriptorPool->build(params, mFrameCount);
        mTemporalDescriptorSet = DescriptorSet::create(mDevice, params, mTemporalSetLayout, mTemporalDescriptorPool, mFrameCount);

        for (int i = 0; i < mFrameCount; ++i) {
            VkDescriptorSet set = mTemporalDescriptorSet->getDescriptorSet(i);
            mTemporalDescriptorSet->updateImage(set, mPrevNormalTemporalParam->mBinding, mPrevNormalTemporalParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mPrevDepthTemporalParam->mBinding, mPrevDepthTemporalParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mCurrNormalTemporalParam->mBinding, mCurrNormalTemporalParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mCurrDepthTemporalParam->mBinding, mCurrDepthTemporalParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mMotionTemporalParam->mBinding, mMotionTemporalParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mHistoryVisibilityParam->mBinding, mHistoryVisibilityParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mVisibilityTemporalParam->mBinding, mVisibilityTemporalParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateImage(set, mMomentReadParam->mBinding, mMomentReadParam->mImageInfos[0]);

            mTemporalDescriptorSet->updateStorageImage(set, mMomentWriteParam->mBinding, mMomentWriteParam->mImageInfos[0]);
            mTemporalDescriptorSet->updateStorageImage(set, mVarianceParam->mBinding, mVarianceParam->mImageInfos[0]);
        }

    }

    void RayTracedShadowPass::createAtrousDS(){
        enum AtrousBinding {
            BINDING_NORMAL = 0,
            BINDING_DEPTH = 1,
            BINDING_INPUT = 2,
            BINDING_OUTPUT = 3   
        };

        std::vector<UniformParameter::Ptr> params;

        auto makeSampler = [&](UniformParameter::Ptr& outParam, AtrousBinding binding, const Texture::Ptr& tex) {
            auto p = UniformParameter::create();
            p->mBinding = static_cast<uint32_t>(binding);
            p->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            p->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
            p->mCount = 1;

            p->mImageInfos.resize(1);
            p->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            p->mImageInfos[0].imageView = tex->getImageView();
            p->mImageInfos[0].sampler = tex->getSampler();

            outParam = p;
            params.push_back(p);
            };

        auto makeStorage = [&](UniformParameter::Ptr& outParam, AtrousBinding binding, const Texture::Ptr& tex) {
            auto p = UniformParameter::create();
            p->mBinding = static_cast<uint32_t>(binding);
            p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            p->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
            p->mCount = 1;

            p->mImageInfos.resize(1);
            p->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            p->mImageInfos[0].imageView = tex->getImageView();
            p->mImageInfos[0].sampler = VK_NULL_HANDLE;

            outParam = p;
            params.push_back(p);
          };

        makeSampler(mAtrousNormalParam, BINDING_NORMAL, mNormal);
        makeSampler(mAtrousDepthParam, BINDING_DEPTH, mDepth);
        makeSampler(mAtrousInputParam, BINDING_INPUT, mVariance);
        makeStorage(mAtrousOutputParam, BINDING_OUTPUT, mAtrous0);

        mAtrousSetLayout = DescriptorSetLayout::create(mDevice);
        mAtrousSetLayout->build(params);
        const int iterations = std::max(mAtrousIterations, 1);
        const int setCount = mFrameCount * iterations;
        mAtrousDescriptorPool = DescriptorPool::create(mDevice);
        mAtrousDescriptorPool->build(params, setCount);
        mAtrousDescriptorSets.resize(iterations);
        for (int i = 0; i < iterations; ++i) {
            mAtrousDescriptorSets[i] = DescriptorSet::create(mDevice, params, mAtrousSetLayout, mAtrousDescriptorPool, mFrameCount);
            for (int j = 0; j < mFrameCount; ++j) {
                VkDescriptorSet set = mAtrousDescriptorSets[i]->getDescriptorSet(j);
                mAtrousDescriptorSets[i]->updateImage(set, BINDING_NORMAL, mAtrousNormalParam->mImageInfos[0]);
                mAtrousDescriptorSets[i]->updateImage(set, BINDING_DEPTH, mAtrousDepthParam->mImageInfos[0]);
                mAtrousDescriptorSets[i]->updateImage(set, BINDING_INPUT, mAtrousInputParam->mImageInfos[0]);
                mAtrousDescriptorSets[i]->updateStorageImage(set, BINDING_OUTPUT, mAtrousOutputParam->mImageInfos[0]);
            }
        }

    }

    void RayTracedShadowPass::createPipeline(const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout) {
        mPipeline = ComputePipeline::create(mDevice);
        auto shader = Shader::create(mDevice, "shaders/shadow/ray_traced_shadow_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
        mPipeline->setShader(shader);
        mPipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), staticSetLayout->getLayout(), mDescriptorSetLayout->getLayout() });
        mPipeline->setPushConstantRange(0, (sizeof(uint32_t)), VK_SHADER_STAGE_COMPUTE_BIT);
        mPipeline->build();

        mTemporalPipeline = ComputePipeline::create(mDevice);
        auto temporalShader = Shader::create(mDevice, "shaders/shadow/temporal_filter_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
        mTemporalPipeline->setShader(temporalShader);
        mTemporalPipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), staticSetLayout->getLayout(), mTemporalSetLayout->getLayout()});
        mTemporalPipeline->setPushConstantRange(0, static_cast<uint32_t>(sizeof(flare::app::ShadowTemporalPC)), VK_SHADER_STAGE_COMPUTE_BIT);
        mTemporalPipeline->build();

        mDenoisePipeline = ComputePipeline::create(mDevice);
        auto atrousShader = Shader::create(mDevice, "shaders/shadow/atrous_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main"); 
        mDenoisePipeline->setShader(atrousShader);
        mDenoisePipeline->setDescriptorSetLayouts({frameSetLayout->getLayout(), staticSetLayout->getLayout(), mAtrousSetLayout->getLayout()});
        mDenoisePipeline->setPushConstantRange(0, static_cast<uint32_t>(sizeof(flare::app::ShadowAtrousPC)), VK_SHADER_STAGE_COMPUTE_BIT);
        mDenoisePipeline->build();
    }

    void RayTracedShadowPass::render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex) {
        rayTracedShadow(cmd, frameSet, staticSet, frameIndex);
        temporalFilter(cmd, frameSet, staticSet, frameIndex);
        atrous(cmd, frameSet, staticSet, frameIndex);
    }

    void RayTracedShadowPass::rayTracedShadow(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, 
                                              int frameIndex) {

        cmd->transitionImageLayout(mVisibility->getImage()->getImage(), mVisibility->getImage()->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        cmd->bindComputePipeline(mPipeline->getPipeline());
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), staticSet->getDescriptorSet(0), 1);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 2);
        cmd->pushConstants(mPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, mBlueNoiseIndex);

        const uint32_t groupX = (mWidth + 7) / 8;
        const uint32_t groupY = (mHeight + 7) / 8;
        cmd->dispatch(groupX, groupY, 1);

        cmd->transitionImageLayout(mVisibility->getImage()->getImage(), mVisibility->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void RayTracedShadowPass::temporalFilter(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex){

        Texture::Ptr historyMomentTex = (mCurrentMomentsIndex == 0) ? mMoment0 : mMoment1;
        Texture::Ptr writeMomentTex = (mCurrentMomentsIndex == 0) ? mMoment1 : mMoment0;
        
        if (!mVarianceInitialized) {
            cmd->transitionImageLayout(mVariance->getImage()->getImage(), mVariance->getImage()->getFormat(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            mVarianceInitialized = true;
        }

        cmd->transitionImageLayout(writeMomentTex->getImage()->getImage(), writeMomentTex->getImage()->getFormat(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        VkDescriptorSet temporalSet = mTemporalDescriptorSet->getDescriptorSet(frameIndex);
        mMomentReadParam->mImageInfos[0].imageView = historyMomentTex->getImageView();
        mMomentReadParam->mImageInfos[0].sampler = historyMomentTex->getSampler();
        mTemporalDescriptorSet->updateImage(temporalSet, mMomentReadParam->mBinding, mMomentReadParam->mImageInfos[0]);

        mMomentWriteParam->mImageInfos[0].imageView = writeMomentTex->getImageView();
        mTemporalDescriptorSet->updateStorageImage(temporalSet, mMomentWriteParam->mBinding, mMomentWriteParam->mImageInfos[0]);

        cmd->bindComputePipeline(mTemporalPipeline->getPipeline());
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mTemporalPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex),0);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mTemporalPipeline->getLayout(), staticSet->getDescriptorSet(0), 1);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mTemporalPipeline->getLayout(), mTemporalDescriptorSet->getDescriptorSet(frameIndex), 2);
        cmd->pushConstants(mTemporalPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, mTemporalPC);

        const uint32_t groupX = (mWidth + 7) / 8;
        const uint32_t groupY = (mHeight + 7) / 8;
        cmd->dispatch(groupX, groupY, 1);

        cmd->transitionImageLayout(writeMomentTex->getImage()->getImage(), writeMomentTex->getImage()->getFormat(),
                                   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        cmd->transitionImageLayout(mVariance->getImage()->getImage(), mVariance->getImage()->getFormat(),
                                   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        mCurrentMomentsIndex = 1 - mCurrentMomentsIndex;
    }

    void RayTracedShadowPass::atrous(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex) {

        Texture::Ptr inputTex = nullptr;
        Texture::Ptr outputTex = nullptr;

        for (int i = 0; i < mAtrousIterations; ++i){
            if (i == 0){
                inputTex = mVariance;
                outputTex = mAtrous0; 
            } else {
                bool ping = (i % 2) == 1;
                inputTex = ping ? mAtrous0 : mAtrous1;
                outputTex = ping ? mAtrous1 : mAtrous0;

                VkDescriptorImageInfo historyReadInfo{};
                historyReadInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                historyReadInfo.imageView = inputTex->getImageView();
                historyReadInfo.sampler = inputTex->getSampler();

                VkDescriptorImageInfo historyWriteInfo{};
                historyWriteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                historyWriteInfo.imageView = outputTex->getImageView();
                historyWriteInfo.sampler = VK_NULL_HANDLE;

                VkDescriptorSet atrousSet = mAtrousDescriptorSets[i]->getDescriptorSet(frameIndex);
                mAtrousDescriptorSets[i]->updateImage(atrousSet, mAtrousInputParam->mBinding, historyReadInfo);
                mAtrousDescriptorSets[i]->updateStorageImage(atrousSet, mAtrousOutputParam->mBinding, historyWriteInfo);
            }

            cmd->transitionImageLayout(outputTex->getImage()->getImage(), outputTex->getImage()->getFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                                       VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            cmd->bindComputePipeline(mDenoisePipeline->getPipeline());
            cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mDenoisePipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
            cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mDenoisePipeline->getLayout(), staticSet->getDescriptorSet(0), 1);
            cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mDenoisePipeline->getLayout(), mAtrousDescriptorSets[i]->getDescriptorSet(frameIndex), 2);

    
            mAtrousPC.stepSize = 1 << i;
            if (i != mAtrousIterations - 1) mAtrousPC.power = 0.0f;
            cmd->pushConstants(mDenoisePipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, mAtrousPC);

            const uint32_t groupX = (mWidth + 7) / 8;
            const uint32_t groupY = (mHeight + 7) / 8;
            cmd->dispatch(groupX, groupY, 1);
            cmd->transitionImageLayout(outputTex->getImage()->getImage(), outputTex->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }
        cmd->transitionImageLayout(mVariance->getImage()->getImage(), mVariance->getImage()->getFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
}