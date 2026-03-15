#pragma once
#include "../../common.h"
#include "device.h"
#include "shader.h"
#include "buffer.h"
#include "ray_tracing_context.h"

namespace flare::vk {

	class RayTracingPipeline {
    public:
        using Ptr = std::shared_ptr<RayTracingPipeline>;
        static Ptr create(const Device::Ptr& device) { return std::make_shared<RayTracingPipeline>(device); }

        RayTracingPipeline(const Device::Ptr& device);
        ~RayTracingPipeline();

        void setDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& layouts) { mSetLayouts = layouts; }
        void setPushConstantRanges(const std::vector<VkPushConstantRange>& ranges) { mPushConstants = ranges; }
        void setMaxRecursionDepth(uint32_t depth) { mMaxRecursionDepth = depth; }

        void setShaders(const Shader::Ptr& raygen, const Shader::Ptr& miss, const Shader::Ptr& closestHit);
        void setShaders(const Shader::Ptr& raygen, const std::vector<Shader::Ptr>& missList, const std::vector<Shader::Ptr>& hitList);
        void setShaders(const Shader::Ptr& raygen, const std::vector<Shader::Ptr>& missList, const std::vector<Shader::Ptr>& hitList, const std::vector<Shader::Ptr>& anyHitList);
        void build();

        [[nodiscard]] VkPipeline       getPipeline() const { return mPipeline; }
        [[nodiscard]] VkPipelineLayout getLayout()   const { return mLayout; }
        [[nodiscard]] const VkStridedDeviceAddressRegionKHR& getRgenRegion()     const { return mRgenRegion; }
        [[nodiscard]] const VkStridedDeviceAddressRegionKHR& getMissRegion()     const { return mMissRegion; }
        [[nodiscard]] const VkStridedDeviceAddressRegionKHR& getHitRegion()      const { return mHitRegion; }
        [[nodiscard]] const VkStridedDeviceAddressRegionKHR& getCallableRegion() const { return mCallableRegion; }

    private:
        void buildPipeline();
        void buildSBT();

    private:
        Device::Ptr mDevice{ nullptr };
        Shader::Ptr mRaygen{ nullptr };
        std::vector<Shader::Ptr> mMissShaders{};
        std::vector<Shader::Ptr> mHitShaders{};
        std::vector<Shader::Ptr> mAnyHitShaders{};

        std::vector<VkPipelineShaderStageCreateInfo> mStages{};
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> mGroups{};
        std::vector<VkDescriptorSetLayout> mSetLayouts{};
        std::vector<VkPushConstantRange>   mPushConstants{};

        VkPipelineLayout mLayout{ VK_NULL_HANDLE };
        VkPipeline mPipeline{ VK_NULL_HANDLE };
        uint32_t   mMaxRecursionDepth{ 1 };

        Buffer::Ptr mSbtRaygen{ nullptr };
        Buffer::Ptr mSbtMiss{ nullptr };
        Buffer::Ptr mSbtHit{ nullptr };

        VkStridedDeviceAddressRegionKHR mRgenRegion{};
        VkStridedDeviceAddressRegionKHR mMissRegion{};
        VkStridedDeviceAddressRegionKHR mHitRegion{};
        VkStridedDeviceAddressRegionKHR mCallableRegion{};
	};
}