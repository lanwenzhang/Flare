#pragma once

#include "../../../../common.h"
#include "../../../../application/type.h"
#include "../../../../platform/graphics/device.h"
#include "../../../../platform/graphics/buffer.h"
#include "../../../../platform/graphics/description.h"
#include "../../../../platform/graphics/descriptor_set.h"

namespace flare::renderer {

    using namespace flare::vk;

    class FrameUniformManager {
    public:
        using Ptr = std::shared_ptr<FrameUniformManager>;
        static Ptr create() { return std::make_shared<FrameUniformManager>(); }

        FrameUniformManager();
        ~FrameUniformManager();

        void init(const Device::Ptr& device, int frameCount);
        void update(const glm::mat4& view, const glm::mat4& projection, const DescriptorSet::Ptr& descriptorSet, int frameIndex);
        void updateLight(const flare::app::LightParam& params, const DescriptorSet::Ptr& descriptorSet, int frameIndex);
        void updateTAA(const flare::app::TAAParams& params, const DescriptorSet::Ptr& descriptorSet, int frameIndex);
        std::vector<UniformParameter::Ptr> getParams() const { return { mVpParam, mLightVpParam, mTAAParam }; }

    private:

        flare::vk::Device::Ptr mDevice{ nullptr };
        flare::vk::UniformParameter::Ptr mVpParam{ nullptr };
        flare::vk::UniformParameter::Ptr mLightVpParam{ nullptr };
        flare::vk::UniformParameter::Ptr mTAAParam{ nullptr };
    };

}