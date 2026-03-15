#pragma once

#include "../../common.h"
#include "instance.h"
#include "surface.h"
#include "vulkan_config.h"
#include "ray_tracing_context.h"

namespace flare::vk {

	class Device {
	public:
		using Ptr = std::shared_ptr<Device>;
		static Ptr create(Instance::Ptr instance, Surface::Ptr surface) { return std::make_shared<Device>(instance, surface); }

		Device(Instance::Ptr instance, Surface::Ptr surface);
		~Device();

		// 1 Create Physical device and logical device
		void pickPhysicalDevice();

		// 1.1 Physical device properties
		int rateDevice(VkPhysicalDevice device);
		bool isDeviceSuitable(VkPhysicalDevice device);

		// 1.2 Physical device queues
		void initQueueFamilies(VkPhysicalDevice device);
		bool isQueueFamilyComplete();

		// 1.3 Logical device
		void createLogicalDevice();
		VkSampleCountFlagBits getMaxUsableSampleCount();
		PFN_vkGetBufferDeviceAddress getBufferDeviceAddressFunction() const;

		const char* resolveModeToString(VkResolveModeFlagBits mode) {
			switch (mode) {
			case VK_RESOLVE_MODE_NONE: return "VK_RESOLVE_MODE_NONE";
			case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT: return "VK_RESOLVE_MODE_SAMPLE_ZERO_BIT";
			case VK_RESOLVE_MODE_AVERAGE_BIT: return "VK_RESOLVE_MODE_AVERAGE_BIT";
			case VK_RESOLVE_MODE_MIN_BIT: return "VK_RESOLVE_MODE_MIN_BIT";
			case VK_RESOLVE_MODE_MAX_BIT: return "VK_RESOLVE_MODE_MAX_BIT";
			default: return "Unknown";
			}
		}

		// 2 For external call
		[[nodiscard]] auto getDevice() const { return mDevice;}
		[[nodiscard]] auto getPhysicalDevice() const { return mPhysicalDevice; }
		[[nodiscard]] auto getGraphicQueueFamily() const { return mGraphicQueueFamily; }
		[[nodiscard]] auto getPresentQueueFamily() const { return mPresentQueueFamily; }
		[[nodiscard]] auto getComputeQueueFamily() const { return mComputeQueueFamily; }
		[[nodiscard]] auto getGraphicQueue() const { return mGraphicQueue;}
		[[nodiscard]] auto getPresentQueue() const { return mPresentQueue; }
		[[nodiscard]] auto getComputeQueue() const { return mComputeQueue; }
		[[nodiscard]] auto& getRT() { return mRTContext; }

	private:
		Instance::Ptr mInstance{ nullptr };
		Surface::Ptr mSurface{ nullptr };
		VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };
		VkDevice mDevice{ VK_NULL_HANDLE };

		std::optional<uint32_t> mGraphicQueueFamily;
		std::optional<uint32_t> mPresentQueueFamily;
		std::optional<uint32_t> mComputeQueueFamily;

		VkQueue mGraphicQueue{ VK_NULL_HANDLE };
		VkQueue mPresentQueue{ VK_NULL_HANDLE };
		VkQueue mComputeQueue{ VK_NULL_HANDLE };

		VkSampleCountFlagBits mSampleCounts{ VK_SAMPLE_COUNT_1_BIT };
		VkResolveModeFlagBits mDepthResolveMode{ VK_RESOLVE_MODE_NONE };

		PFN_vkGetBufferDeviceAddress fpGetBufferDeviceAddress = nullptr;
		RayTracingContext mRTContext{};
	};
}