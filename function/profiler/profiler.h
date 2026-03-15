#pragma once
#include <memory>
#include <cstdint>
#include "../../platform/graphics/command_buffer.h"
#include "../../platform/graphics/buffer.h"
#include "../../external/imgui/imgui.h"                     
#include "../../external/imgui/imgui_impl_vulkan.h"
#include "../../external/imgui/imgui_impl_glfw.h"

namespace flare::renderer {
    using namespace flare::vk;

    struct ProfilerSection{
        enum Enum : uint32_t{
            Frame = 0,
            DepthPrePass,
            DepthPyramidPass,
            CullingPass,
            GBufferPass,
            MotionVectorPass,
            RayTracedShadowPass,
            DDGIPass,
            GTAOPass,
            LightingPass,
            TAAPass,
            ToneMappingPass,
            Count
        };
    };

    class Profiler{
    public:
        using Ptr = std::shared_ptr<Profiler>;
        static Ptr create(const Device::Ptr& device){return std::make_shared<Profiler>(device);}

        Profiler(const Device::Ptr& device);
        ~Profiler();

        void setEnabled(bool enabled) { mEnabled = enabled; }
        bool isEnabled() const { return mEnabled; }

        void resolvePreviousFrame(int frameIndex);
        void beginFrame(const CommandBuffer::Ptr& cmd, int frameIndex);
        void endFrame(const CommandBuffer::Ptr& cmd, int frameIndex);
        void beginSection(const CommandBuffer::Ptr& cmd, ProfilerSection::Enum section, int frameIndex);
        void endSection(const CommandBuffer::Ptr& cmd, ProfilerSection::Enum section, int frameIndex);
        void buildUI();

    private:
        Device::Ptr mDevice{ nullptr };
        bool mEnabled{ true };

        uint32_t mActiveBank{ 0 };
        VkQueryPool mQueryPool{ VK_NULL_HANDLE };
        float mTimestampPeriod{ 1.0f };

        std::array<double, ProfilerSection::Count> mTimerValues{};
        std::array<bool, ProfilerSection::Count * 2> mUsed{}; 
    };

    class ProfilerScope {
    public:
        ProfilerScope(Profiler* p, const CommandBuffer::Ptr& cmd, ProfilerSection::Enum sec, int frameIndex){
            mProfiler = p, mCmd = cmd, mSec = sec, mFrameIndex = frameIndex;
            if (mProfiler && mProfiler->isEnabled())
                mProfiler->beginSection(mCmd, mSec, mFrameIndex);
        }
        ~ProfilerScope() {
            if (mProfiler && mProfiler->isEnabled())
                mProfiler->endSection(mCmd, mSec, mFrameIndex);
        }
    private:
        Profiler* mProfiler{};
        CommandBuffer::Ptr mCmd{};
        ProfilerSection::Enum mSec{};
        int mFrameIndex{};
    };
}