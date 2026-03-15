#include "descriptor_pool.h"

namespace flare::vk {

	DescriptorPool::DescriptorPool(const Device::Ptr& device) {
		
		mDevice = device;
	}

	DescriptorPool::~DescriptorPool() {

		if (mPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(mDevice->getDevice(), mPool, nullptr);
		}
	}

	void DescriptorPool::build(std::vector<UniformParameter::Ptr>& params, const int& frameCount) {

		// 1 Get the number of uniform buffer
		uint32_t uniformBufferCount = 0;
		uint32_t textureCount = 0;
		uint32_t storageBufferCount = 0;
		uint32_t storageImageCount = 0;
		uint32_t accelerationStructureCount = 0;

		for (const auto& param : params) {
			if (!param) continue;

			switch (param->mDescriptorType) {
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				uniformBufferCount += param->mCount;
				break;

			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				textureCount += param->mCount;
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				storageBufferCount += param->mCount;
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				storageImageCount += param->mCount;
				break;
			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				storageImageCount += 0;
				accelerationStructureCount += param->mCount;
				break;
			default:
				std::cerr << "[DescriptorPool] Warning: Unsupported descriptor type "
					<< param->mDescriptorType << std::endl;
				break;
			}
		}

		// 2 Pool size
		std::vector<VkDescriptorPoolSize> poolSizes{};

		if (uniformBufferCount > 0) {
			poolSizes.push_back({
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				uniformBufferCount * frameCount
				});
		}
		if (textureCount > 0) {
			poolSizes.push_back({
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				textureCount * frameCount
				});
		}
		if (storageBufferCount > 0) {
			poolSizes.push_back({
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				storageBufferCount * frameCount
				});
		}
		if (storageImageCount > 0) {
			poolSizes.push_back({
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				storageImageCount * frameCount
				});
		}
		if (accelerationStructureCount > 0) {
			poolSizes.push_back({
				VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
				accelerationStructureCount * frameCount
				});
		}
		if (poolSizes.empty()) {
			throw std::runtime_error("Error: DescriptorPool - no valid pool sizes.");
		}


		// 3 Pool create info
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = static_cast<uint32_t>(frameCount) + 1;
		createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

		// 4 Create pool
		if (vkCreateDescriptorPool(mDevice->getDevice(), &createInfo, nullptr, &mPool) != VK_SUCCESS) {

			throw std::runtime_error("Error: failed to create Descriptor pool");
		}
	}

}