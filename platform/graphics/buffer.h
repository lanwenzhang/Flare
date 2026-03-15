#pragma once

#include "../../common.h"
#include "device.h"

namespace flare::vk {

	class Buffer {
	public:

		using Ptr = std::shared_ptr<Buffer>;
		static Ptr create(const Device::Ptr& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool supportDeviceAddress = false) {
			
			return std::make_shared<Buffer>(device, size, usage, properties, supportDeviceAddress); 
		}

	public:

		static Ptr createVertexBuffer(const Device::Ptr& device, VkDeviceSize size, const void* pData, bool supportDeviceAddress = false);
		static Ptr createIndexBuffer(const Device::Ptr& device, VkDeviceSize size, const void* pData, bool supportDeviceAddress = false);
		static Ptr createUniformBuffer(const Device::Ptr& device, VkDeviceSize size, const void* pData = nullptr);
		static Ptr createStageBuffer(const Device::Ptr& device, VkDeviceSize size, const void* pData = nullptr);
		static Ptr createStorageBuffer(const Device::Ptr& device, VkDeviceSize size, const void* pData, bool supportDeviceAddress);
		static Ptr createHostVisibleStorageBuffer(const Device::Ptr& device, VkDeviceSize size, const void* pData = nullptr);

	public:

		Buffer(const Device::Ptr &device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool supportDeviceAddress = false);
		~Buffer();
		
		void updateBufferByMap(const void* data, size_t size);
		void updateBufferByStage(const void* data, size_t size);
		void updateBufferByStage(const void* data, size_t size, size_t offset);
		void copyBuffer(const VkBuffer& srcBuffer, const VkBuffer& dstBuffer, VkDeviceSize size);
		void copyBuffer(const VkBuffer& srcBuffer, const VkBuffer& dstBuffer, VkDeviceSize size, size_t offset);

		void* map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		void unmap();

		[[nodiscard]] auto getDeviceAddress() const {

			if (!mDevice) throw std::runtime_error("getDeviceAddress: mDevice is null");
			if (mBuffer == VK_NULL_HANDLE) throw std::runtime_error("getDeviceAddress: mBuffer is VK_NULL_HANDLE");

			VkBufferDeviceAddressInfo info{};
			info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
			info.buffer = mBuffer;

			VkDevice deviceHandle = mDevice->getDevice();
			if (deviceHandle == VK_NULL_HANDLE) {
				std::cerr << "[getDeviceAddress] device handle is VK_NULL_HANDLE!\n";
				throw std::runtime_error("getDeviceAddress: device handle is null");
			}

			PFN_vkGetBufferDeviceAddress func = mDevice->getBufferDeviceAddressFunction();
			if (!func) {
				throw std::runtime_error("getDeviceAddress: vkGetBufferDeviceAddress function not loaded");
			}

			VkDeviceAddress address = func(deviceHandle, &info);
			if (address == 0) {
				throw std::runtime_error("getDeviceAddress: vkGetBufferDeviceAddress returned 0 (missing flags?)");
			}
			return address;

		}
		[[nodiscard]] auto getBuffer() const { return mBuffer; }
		[[nodiscard]] auto& getBufferInfo() { return mBufferInfo; }
		[[nodiscard]] auto getVkDeviceMemory() const {
			return mBufferMemory;
		}
	private:

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	private:

		VkBuffer mBuffer{ VK_NULL_HANDLE };
		VkDeviceMemory mBufferMemory{ VK_NULL_HANDLE };
		Device::Ptr mDevice{ nullptr };
		VkDescriptorBufferInfo mBufferInfo{};
	};

}