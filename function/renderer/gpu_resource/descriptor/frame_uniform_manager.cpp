#include "frame_uniform_manager.h"

namespace flare::renderer {

    FrameUniformManager::FrameUniformManager(){}
    FrameUniformManager::~FrameUniformManager(){}

    void FrameUniformManager::init(const Device::Ptr& device, int frameCount) {
       
        mDevice = device;

        mVpParam = flare::vk::UniformParameter::create();
        mVpParam->mBinding = 0;
        mVpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mVpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        mVpParam->mCount = 1;
        mVpParam->mSize = sizeof(flare::app::VPMatrices);

        mLightVpParam = flare::vk::UniformParameter::create();
        mLightVpParam->mBinding = 1;
        mLightVpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mLightVpParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        mLightVpParam->mCount = 1;
        mLightVpParam->mSize = sizeof(flare::app::LightParam);

        mTAAParam = flare::vk::UniformParameter::create();
        mTAAParam->mBinding = 2;
        mTAAParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mTAAParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        mTAAParam->mCount = 1;
        mTAAParam->mSize = sizeof(flare::app::TAAParams);

        for (int i = 0; i < frameCount; ++i) {
            auto camBuffer = flare::vk::Buffer::createUniformBuffer(device, mVpParam->mSize, nullptr);
            mVpParam->mBuffers.push_back(camBuffer);

            auto lightBuffer = flare::vk::Buffer::createUniformBuffer(device, mLightVpParam->mSize, nullptr);
            mLightVpParam->mBuffers.push_back(lightBuffer);

            auto taaBuffer = flare::vk::Buffer::createUniformBuffer(device, mTAAParam->mSize, nullptr);
            mTAAParam->mBuffers.push_back(taaBuffer);
        }
    }

    void FrameUniformManager::update(const glm::mat4& view, const glm::mat4& projection, const DescriptorSet::Ptr& descriptorSet, int frameIndex) {
        
        flare::app ::VPMatrices vp{};
        vp.mViewMatrix = view;
        vp.mProjectionMatrix = projection;

        mVpParam->mBuffers[frameIndex]->updateBufferByMap(&vp, sizeof(flare::app::VPMatrices));

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = mVpParam->mBuffers[frameIndex]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(flare::app::VPMatrices);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->getDescriptorSet(frameIndex);
        write.dstBinding = mVpParam->mBinding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
    }

    void FrameUniformManager::updateLight(const flare::app::LightParam& params, const flare::vk::DescriptorSet::Ptr& descriptorSet, int frameIndex) {

        mLightVpParam->mBuffers[frameIndex]->updateBufferByMap(&params, sizeof(flare::app::LightParam));

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = mLightVpParam->mBuffers[frameIndex]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(flare::app::LightParam);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->getDescriptorSet(frameIndex);
        write.dstBinding = mLightVpParam->mBinding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write,0, nullptr);

    }

    void FrameUniformManager::updateTAA(const flare::app::TAAParams& params, const DescriptorSet::Ptr& descriptorSet, int frameIndex) {
        
        mTAAParam->mBuffers[frameIndex]->updateBufferByMap((void*)&params, sizeof(flare::app::TAAParams));

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = mTAAParam->mBuffers[frameIndex]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(flare::app::TAAParams);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->getDescriptorSet(frameIndex);
        write.dstBinding = mTAAParam->mBinding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
    }
}
