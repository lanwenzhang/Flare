#include "ray_tracing_pipeline.h"

namespace flare::vk {

    RayTracingPipeline::RayTracingPipeline(const Device::Ptr& device) {
        mDevice = device;
        mCallableRegion = { 0, 0, 0 };
    }

    RayTracingPipeline::~RayTracingPipeline() {
        if (mPipeline) {
            vkDestroyPipeline(mDevice->getDevice(), mPipeline, nullptr);
            mPipeline = VK_NULL_HANDLE;
        }
        if (mLayout) {
            vkDestroyPipelineLayout(mDevice->getDevice(), mLayout, nullptr);
            mLayout = VK_NULL_HANDLE;
        }
    }

    void RayTracingPipeline::setShaders(const Shader::Ptr& raygen, const Shader::Ptr& miss, const Shader::Ptr& closestHit) {
        mRaygen = raygen;
        mMissShaders = miss ? std::vector<Shader::Ptr>{ miss } : std::vector<Shader::Ptr>{};
        mHitShaders = closestHit ? std::vector<Shader::Ptr>{ closestHit } : std::vector<Shader::Ptr>{};
    }

    void RayTracingPipeline::setShaders(const Shader::Ptr& raygen, const std::vector<Shader::Ptr>& missList, const std::vector<Shader::Ptr>& hitList) {
        mRaygen = raygen;
        mMissShaders = missList;
        mHitShaders = hitList;
    }

    void RayTracingPipeline::setShaders(const Shader::Ptr& raygen, const std::vector<Shader::Ptr>& missList, const std::vector<Shader::Ptr>& hitList, const std::vector<Shader::Ptr>& anyHitList){
        mRaygen = raygen;
        mMissShaders = missList;
        mHitShaders = hitList;
        mAnyHitShaders = anyHitList;
    }

    void RayTracingPipeline::build() {
        buildPipeline();
        buildSBT();
    }

    static VkPipelineShaderStageCreateInfo makeStage(const Shader::Ptr& sh) {
        VkPipelineShaderStageCreateInfo s{};
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s.stage = sh->getShaderStage();
        s.module = sh->getShaderModule();
        s.pName = sh->getShaderEntryPoint().c_str();
        return s;
    }

    void RayTracingPipeline::buildPipeline() {
        if (!mRaygen) {
            throw std::runtime_error("RayTracingPipeline: raygen shader not set");
        }
        mStages.clear();
        mGroups.clear();

        const uint32_t rgenIndex = static_cast<uint32_t>(mStages.size());
        mStages.push_back(makeStage(mRaygen));

        VkRayTracingShaderGroupCreateInfoKHR rgenGroup{};
        rgenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        rgenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        rgenGroup.generalShader = rgenIndex;
        rgenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        rgenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        rgenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        mGroups.push_back(rgenGroup);

        for (const auto& ms : mMissShaders) {
            const uint32_t idx = static_cast<uint32_t>(mStages.size());
            mStages.push_back(makeStage(ms));
            VkRayTracingShaderGroupCreateInfoKHR g{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
            g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            g.generalShader = idx;
            g.closestHitShader = VK_SHADER_UNUSED_KHR;
            g.anyHitShader = VK_SHADER_UNUSED_KHR;
            g.intersectionShader = VK_SHADER_UNUSED_KHR;
            mGroups.push_back(g);
        }
        // hit
        for (size_t i = 0; i < mHitShaders.size(); ++i) {
            const auto& ch = mHitShaders[i];
            const uint32_t chIdx = static_cast<uint32_t>(mStages.size());
            mStages.push_back(makeStage(ch));

            uint32_t ahIdx = VK_SHADER_UNUSED_KHR;
            if (i < mAnyHitShaders.size() && mAnyHitShaders[i]) {
                ahIdx = static_cast<uint32_t>(mStages.size());
                mStages.push_back(makeStage(mAnyHitShaders[i]));
            }
            VkRayTracingShaderGroupCreateInfoKHR g{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
            g.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            g.generalShader = VK_SHADER_UNUSED_KHR;
            g.closestHitShader = chIdx;
            g.anyHitShader = ahIdx;
            g.intersectionShader = VK_SHADER_UNUSED_KHR;
            mGroups.push_back(g);
        }

        // layout
        if (mLayout) {
            vkDestroyPipelineLayout(mDevice->getDevice(), mLayout, nullptr);
            mLayout = VK_NULL_HANDLE;
        }
        VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.setLayoutCount = static_cast<uint32_t>(mSetLayouts.size());
        layoutInfo.pSetLayouts = mSetLayouts.data();
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(mPushConstants.size());
        layoutInfo.pPushConstantRanges = mPushConstants.data();
        vkCreatePipelineLayout(mDevice->getDevice(), &layoutInfo, nullptr, &mLayout);

        // create RT pipeline
        VkRayTracingPipelineCreateInfoKHR ci{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
        ci.stageCount = static_cast<uint32_t>(mStages.size());
        ci.pStages = mStages.data();
        ci.groupCount = static_cast<uint32_t>(mGroups.size());
        ci.pGroups = mGroups.data();
        ci.maxPipelineRayRecursionDepth = mMaxRecursionDepth;
        ci.layout = mLayout;
        if (mPipeline) {
            vkDestroyPipeline(mDevice->getDevice(), mPipeline, nullptr);
            mPipeline = VK_NULL_HANDLE;
        }
        mDevice->getRT().vkCreateRayTracingPipelinesKHR(
            mDevice->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline);
    }

    void RayTracingPipeline::buildSBT(){
        auto& rt = mDevice->getRT();

        const uint32_t handleSize = rt.handleSize;
        const uint32_t handleStride = rt.handleSizeAligned;
        const uint32_t groupCount = static_cast<uint32_t>(mGroups.size());
        const uint32_t rgenCount = 1;
        const uint32_t missCount = static_cast<uint32_t>(mMissShaders.size());
        const uint32_t hitCount = groupCount - rgenCount - missCount;

        std::vector<uint8_t> handles(handleSize * groupCount);
        rt.vkGetRayTracingShaderGroupHandlesKHR(mDevice->getDevice(), mPipeline,  0, groupCount, handles.size(), handles.data());

        auto mkSbtBuffer = [&](uint32_t recordCount, const uint8_t* srcBegin)->Buffer::Ptr {
            if (recordCount == 0) return nullptr;
            const VkDeviceSize sbtSize = static_cast<VkDeviceSize>(recordCount) * handleStride;
            auto buf = Buffer::create(mDevice, sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);

            std::vector<uint8_t> tmp(static_cast<size_t>(sbtSize), 0);
            for (uint32_t i = 0; i < recordCount; ++i) {
                std::memcpy(tmp.data() + i * handleStride, srcBegin + i * handleSize, handleSize);
            }
            buf->updateBufferByMap(tmp.data(), tmp.size());
            return buf;
        };

        const uint8_t* rgenSrc = handles.data();
        const uint8_t* missSrc = rgenSrc + handleSize * rgenCount;
        const uint8_t* hitSrc = missSrc + handleSize * missCount;

        mSbtRaygen = mkSbtBuffer(rgenCount, rgenSrc);
        mSbtMiss = mkSbtBuffer(missCount, missSrc);
        mSbtHit = mkSbtBuffer(hitCount, hitSrc);

        auto mkRegion = [&](const Buffer::Ptr& b, uint32_t count)->VkStridedDeviceAddressRegionKHR {
            if (!b || count == 0) return VkStridedDeviceAddressRegionKHR{ 0, 0, 0 };
            VkStridedDeviceAddressRegionKHR r{};
            r.deviceAddress = b->getDeviceAddress();
            r.stride = handleStride;
            r.size = static_cast<VkDeviceSize>(count) * handleStride;
            return r;
        };

        mRgenRegion = mkRegion(mSbtRaygen, rgenCount);
        mMissRegion = mkRegion(mSbtMiss, missCount);
        mHitRegion = mkRegion(mSbtHit, hitCount);
        mCallableRegion = { 0, 0, 0 };
    }
}