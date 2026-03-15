#include "profiler.h"


namespace flare::renderer {

    static constexpr const char* kProfilerSectionNames[flare::renderer::ProfilerSection::Count] = {
        "Frame",
        "Depth Pre",
        "Depth Pyramid",
        "Culling",
        "GBuffer",
        "Motion Vectors",
        "RT Shadow",
        "DDGI",
        "GTAO",
        "Lighting",
        "TAA",
        "Tone Mapping",
    };

    Profiler::Profiler(const Device::Ptr& device) {
        mDevice = device;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(mDevice->getPhysicalDevice(), &props);
        mTimestampPeriod = props.limits.timestampPeriod;

        VkQueryPoolCreateInfo qpci{};
        qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpci.queryCount = ProfilerSection::Count * 4;
        vkCreateQueryPool(mDevice->getDevice(), &qpci, nullptr, &mQueryPool);

        mTimerValues.fill(0.0);
        mUsed.fill(false);
    }

    Profiler::~Profiler() {
        if (mQueryPool != VK_NULL_HANDLE){
            vkDestroyQueryPool(mDevice->getDevice(), mQueryPool, nullptr);
        }
    }

    void Profiler::resolvePreviousFrame(int frameIndex){
        mActiveBank = 1u - mActiveBank; 
        if (!mEnabled)
            return;

        const uint32_t readBank = mActiveBank;
        for (uint32_t section = 0; section < ProfilerSection::Count; ++section){
            const uint32_t usedIndex = readBank * ProfilerSection::Count + section;
            if (!mUsed[usedIndex]){
                mTimerValues[section] = 0.0;
                continue;
            }

            const uint32_t baseQuery = readBank * (ProfilerSection::Count * 2) + section * 2;
            uint64_t timestamps[2] = {};
            VkResult r = vkGetQueryPoolResults(mDevice->getDevice(), mQueryPool, baseQuery, 2,
                sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

            if (r == VK_SUCCESS){
                uint64_t dt = timestamps[1] - timestamps[0];
                double ns = double(dt) * double(mTimestampPeriod);
                mTimerValues[section] = ns / 1e6;
            }
            else{
                mTimerValues[section] = 0.0;
            }
            mUsed[usedIndex] = false;
        }
    }

    void Profiler::beginFrame(const CommandBuffer::Ptr& cmd, int frameIndex){
        if (!mEnabled) return;

        const uint32_t writeBank = 1u - mActiveBank; 
        const uint32_t firstQuery = writeBank * (ProfilerSection::Count * 2);
        const uint32_t queryCount = ProfilerSection::Count * 2;
        vkCmdResetQueryPool(cmd->getCommandBuffer(), mQueryPool, firstQuery, queryCount);
        beginSection(cmd, ProfilerSection::Frame, 0);
    }

    void Profiler::endFrame(const CommandBuffer::Ptr& cmd, int frameIndex){
        if (!mEnabled) return;
        endSection(cmd, ProfilerSection::Frame, 0);
    }

    void Profiler::beginSection(const CommandBuffer::Ptr& cmd, ProfilerSection::Enum section, int frameIndex){
        if (!mEnabled) return;
        const uint32_t writeBank = 1u - mActiveBank;
        const uint32_t usedIndex = writeBank * ProfilerSection::Count + uint32_t(section);
        mUsed[usedIndex] = true;
        const uint32_t baseQuery = writeBank * (ProfilerSection::Count * 2) + uint32_t(section) * 2;
        vkCmdWriteTimestamp(cmd->getCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, mQueryPool, baseQuery + 0);
    }

    void Profiler::endSection(const CommandBuffer::Ptr& cmd, ProfilerSection::Enum section, int frameIndex){
        if (!mEnabled) return;
        const uint32_t writeBank = 1u - mActiveBank;
        const uint32_t baseQuery = writeBank * (ProfilerSection::Count * 2) + uint32_t(section) * 2;
        vkCmdWriteTimestamp(cmd->getCommandBuffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mQueryPool, baseQuery + 1);
    }

    void Profiler::buildUI() {
        if (!mEnabled) return;
        if (ImGui::BeginTable("GPU Frame Time", 2)) {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableHeadersRow();

            for (uint32_t s = 0; s < ProfilerSection::Count; ++s){
                if (s == ProfilerSection::Frame)
                    continue;

                double ms = mTimerValues[s];
                if (ms <= 0.0)
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", kProfilerSectionNames[s]);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", ms);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();

            const double frameMs = mTimerValues[ProfilerSection::Frame];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 80, 255));
            ImGui::Text("Frame");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", frameMs);
            ImGui::PopStyleColor();
            ImGui::EndTable();
        }
    }
}