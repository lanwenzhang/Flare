#include "descriptor_set.h"

namespace flare::vk {

    DescriptorSet::DescriptorSet(const Device::Ptr& device, const std::vector<UniformParameter::Ptr> params, const DescriptorSetLayout::Ptr& layout,
                                 const DescriptorPool::Ptr& pool, int frameCount){
        mDevice = device;
        // 1 max binding variables
        uint32_t variableCount = 0;
        for (auto& param : params) {
            if (param && param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && param->mCount > 1) {
                variableCount = std::max(variableCount, param->mCount);
            }
        }
        // 2 allocate space
        // 2.1 count info
        std::vector<uint32_t> counts;
        VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{};
        if (variableCount > 0) {

            counts.resize(frameCount, variableCount);
            countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            countInfo.descriptorSetCount = frameCount;
            countInfo.pDescriptorCounts = counts.data();
        }
        std::vector<VkDescriptorSetLayout> layouts(frameCount, layout->getLayout());
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool->getPool();
        allocInfo.descriptorSetCount = frameCount;
        allocInfo.pSetLayouts = layouts.data();

        if (variableCount > 0) {
            allocInfo.pNext = &countInfo;
        }
        else {
            allocInfo.pNext = nullptr;
        }

        mDescriptorSets.resize(frameCount);
        if (vkAllocateDescriptorSets(mDevice->getDevice(), &allocInfo, mDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Error: failed to allocate descriptor sets");
        }


        std::vector<std::vector<VkDescriptorBufferInfo>> allBufferInfos(params.size());
        for (int i = 0; i < frameCount; ++i) {

            std::vector<VkWriteDescriptorSet> writes;
            size_t paramIdx = 0;
            for (auto& param : params) {
                if (!param || param->mCount == 0) {
                    ++paramIdx;
                    continue;
                }
                if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                    param->mDescriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                    if (param->mBuffers.empty()) {
                        ++paramIdx;
                        continue;
                    }
                    auto& bufferInfos = allBufferInfos[paramIdx];
                    bufferInfos.clear();
                    for (auto& buffer : param->mBuffers) {
                        if (!buffer) continue;
                        VkDescriptorBufferInfo bufferInfo{};
                        bufferInfo.buffer = buffer->getBuffer();
                        bufferInfo.offset = 0;
                        bufferInfo.range = param->mSize;
                        bufferInfos.push_back(bufferInfo);
                    }
                    if (bufferInfos.empty()) {
                        ++paramIdx;
                        continue;
                    }
                    uint32_t descriptorCount = std::min<uint32_t>(static_cast<uint32_t>(bufferInfos.size()), param->mCount);
                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = mDescriptorSets[i];
                    write.dstBinding = param->mBinding;
                    write.descriptorType = param->mDescriptorType;
                    write.descriptorCount = param->mCount;
                    write.pBufferInfo = bufferInfos.data();
                    writes.push_back(write);
                }
                else if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                    if (param->mImageInfos.empty()) {
                        ++paramIdx;
                        continue;
                    }
                    uint32_t descriptorCount = std::min<uint32_t>(static_cast<uint32_t>(param->mImageInfos.size()), param->mCount);
                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = mDescriptorSets[i];
                    write.dstBinding = param->mBinding;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    write.descriptorCount = descriptorCount;
                    write.pImageInfo = param->mImageInfos.data();
                    writes.push_back(write);
                }
                else if (param->mDescriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) {
                     if (param->mAccelerationStructure.empty()) { ++paramIdx; continue; }

                    const uint32_t descriptorCount = std::min<uint32_t>(
                        static_cast<uint32_t>(param->mAccelerationStructure.size()),
                        param->mCount ? param->mCount : 1u
                    );

                    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
                    };
                    asInfo.accelerationStructureCount = descriptorCount;
                    asInfo.pAccelerationStructures = param->mAccelerationStructure.data();

                    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                    write.dstSet = mDescriptorSets[i];
                    write.dstBinding = param->mBinding;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                    write.descriptorCount = descriptorCount;
                    write.pNext = &asInfo;
                    writes.push_back(write);
                }
                ++paramIdx;
            }
            if (!writes.empty()) {
                vkUpdateDescriptorSets(mDevice->getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            }
        }
    }

    void DescriptorSet::updateImage(const VkDescriptorSet& descriptorSet, uint32_t binding, const VkDescriptorImageInfo& imageInfo) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
    }

    void DescriptorSet::updateStorageImage(const VkDescriptorSet& descriptorSet, uint32_t binding, const VkDescriptorImageInfo& imageInfo) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
    }

    void DescriptorSet::updateBuffer(const VkDescriptorSet& descriptorSet, uint32_t binding, const VkDescriptorBufferInfo& bufferInfo) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
    }

	DescriptorSet::~DescriptorSet() {}
}