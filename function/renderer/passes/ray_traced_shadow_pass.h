#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_pool.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../gpu_resource/buffer/geometry_buffer.h"
#include "../gpu_resource/texture/texture.h"
#include "geometry_pass.h"
#include "motion_vector_pass.h"

namespace flare::renderer {

    class RayTracedShadowPass {
    public:
        using Ptr = std::shared_ptr<RayTracedShadowPass>;
        static Ptr create(Device::Ptr& device) { return std::make_shared<RayTracedShadowPass>(device); }

        RayTracedShadowPass(Device::Ptr& device) { mDevice = device; }
        ~RayTracedShadowPass();

        void init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const GeometryPass::Ptr& geometryPass,
                  const MotionVectorPass::Ptr& motionVectorPass,
                  const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout, int frameCount);
        void render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex);
        [[nodiscard]] auto getFilteredVisibility() const{
            if (mAtrousIterations <= 0) return mVariance;
            int last = (mAtrousIterations - 1) % 2;
            return (last == 0) ? mAtrous0 : mAtrous1;
        }
        [[nodiscard]] auto getVisibility() const { return mVisibility; }
        [[nodiscard]] auto getHistoryVisibility() const { return mHistoryVisibility; }

    private:
        void createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph);
        void createShadowDS();
        void createTemporalFilterDS();
        void createAtrousDS();
        void createPipeline(const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout);
        void rayTracedShadow(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet,
                             const DescriptorSet::Ptr& staticSet, int frameIndex);
        void temporalFilter(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, 
                            const DescriptorSet::Ptr& staticSet, int frameIndex);
        void atrous(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet,
                    const DescriptorSet::Ptr& staticSet, int frameIndex);
    public:
        uint32_t mBlueNoiseIndex{ 0 };
        flare::app::ShadowTemporalPC mTemporalPC;
        flare::app::ShadowAtrousPC mAtrousPC;
    
    private:
        unsigned int mWidth{ 1200 };
        unsigned int mHeight{ 800 };

        int mFrameCount{ 0 };

        Device::Ptr mDevice{ nullptr };
        ComputePipeline::Ptr mPipeline{ nullptr };
        ComputePipeline::Ptr  mDenoisePipeline{ nullptr };
        ComputePipeline::Ptr  mTemporalPipeline{ nullptr };

        // rt shadow pass
        Texture::Ptr mBlueNoiseSobol{ nullptr };
        std::array<Texture::Ptr, 9> mBlueNoiseScramblingRanking{};

        UniformParameter::Ptr mNormalParam{ nullptr };
        UniformParameter::Ptr mDepthParam{ nullptr };
        UniformParameter::Ptr mVisibilityParam{ nullptr };
        UniformParameter::Ptr mBlueNoiseSobolParam{ nullptr };
        UniformParameter::Ptr mBlueNoiseScramblingParam{ nullptr };

        DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
        DescriptorPool::Ptr      mDescriptorPool{ nullptr };
        DescriptorSet::Ptr       mDescriptorSet{ nullptr };

        // temporal pass
        Texture::Ptr mPrevNormal{ nullptr };
        Texture::Ptr mPrevDepth{ nullptr };
        Texture::Ptr mNormal{ nullptr };
        Texture::Ptr mDepth{ nullptr };
        Texture::Ptr mMotionVector{ nullptr };

        Texture::Ptr mHistoryVisibility{ nullptr };
        Texture::Ptr mVisibility{ nullptr };
        int mCurrentMomentsIndex = 0;
        Texture::Ptr mMoment0{ nullptr };
        Texture::Ptr mMoment1{ nullptr };
        Texture::Ptr mVariance{ nullptr };
        bool mVarianceInitialized = false;

        UniformParameter::Ptr     mPrevNormalTemporalParam{ nullptr };
        UniformParameter::Ptr     mPrevDepthTemporalParam{ nullptr };
        UniformParameter::Ptr     mCurrNormalTemporalParam{ nullptr };
        UniformParameter::Ptr     mCurrDepthTemporalParam{ nullptr };
        UniformParameter::Ptr     mMotionTemporalParam{ nullptr };
        UniformParameter::Ptr     mHistoryVisibilityParam{ nullptr };
        UniformParameter::Ptr     mVisibilityTemporalParam{ nullptr };
        UniformParameter::Ptr     mMomentReadParam{ nullptr };
        UniformParameter::Ptr     mMomentWriteParam{ nullptr };
        UniformParameter::Ptr     mVarianceParam{ nullptr };

        DescriptorSetLayout::Ptr  mTemporalSetLayout{ nullptr };
        DescriptorPool::Ptr       mTemporalDescriptorPool{ nullptr };
        DescriptorSet::Ptr        mTemporalDescriptorSet{ nullptr };

        // a-torus pass
        Texture::Ptr mAtrous0{ nullptr };
        Texture::Ptr mAtrous1{ nullptr };
        int mAtrousIterations{ 4 };
        UniformParameter::Ptr    mAtrousNormalParam{ nullptr };
        UniformParameter::Ptr    mAtrousDepthParam{ nullptr };
        UniformParameter::Ptr    mAtrousInputParam{ nullptr };
        UniformParameter::Ptr    mAtrousOutputParam{ nullptr };

        DescriptorSetLayout::Ptr mAtrousSetLayout{ nullptr };
        DescriptorPool::Ptr      mAtrousDescriptorPool{ nullptr };
        std::vector<DescriptorSet::Ptr> mAtrousDescriptorSets{};
    };
}
