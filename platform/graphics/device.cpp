#include "device.h"

namespace flare::vk {

	Device::Device(Instance::Ptr instance, Surface::Ptr surface) {
		mInstance = instance;
		mSurface = surface;
		pickPhysicalDevice();
		initQueueFamilies(mPhysicalDevice);
		createLogicalDevice();
		mRTContext.init(mPhysicalDevice, mDevice);
	}

	Device::~Device() {
		vkDestroyDevice(mDevice, nullptr);
		mSurface.reset();
		mInstance.reset();
	}

	void Device::pickPhysicalDevice() {
		// 1 Get physical device
		// 1.1 Get the number of physical device
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(mInstance->getInstance(), &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw std::runtime_error("Error: failed to enumeratePhysicalDevice");
		}

		// 1.2 Put physical devices into the container
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(mInstance->getInstance(), &deviceCount, devices.data());

		// 2 Rate physical device
		std::multimap<int, VkPhysicalDevice> candidates;
		for (const auto& device : devices) {
			int score = rateDevice(device);
			candidates.insert(std::make_pair(score, device));
		}
		if (candidates.rbegin()->first > 0 && isDeviceSuitable(candidates.rbegin()->second)) {
			mPhysicalDevice = candidates.rbegin()->second;
		}
		if (mPhysicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("Error:failed to get physical device");
		}
	}

	int Device::rateDevice(VkPhysicalDevice device) {
		int score = 0;
		// 1 Get device type verison
		VkPhysicalDeviceProperties deviceProp;
		vkGetPhysicalDeviceProperties(device, &deviceProp);
		// 2 Get device features 
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		if (deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1000;
		}
		score += deviceProp.limits.maxImageDimension2D;
		if (!deviceFeatures.geometryShader) {
			return 0;
		}
		return score;
	}

	bool Device::isDeviceSuitable(VkPhysicalDevice device) {

		// 1 Get device type verison
		VkPhysicalDeviceProperties deviceProp;
		vkGetPhysicalDeviceProperties(device, &deviceProp);

		// 2 Get device features
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceRequiredExtensions.begin(), deviceRequiredExtensions.end());
		for (const auto& ext : availableExtensions) {
			requiredExtensions.erase(ext.extensionName);
		}
		if (!requiredExtensions.empty()) {
			std::cerr << "Missing required device extensions!\n";
			return false;
		}

		// 4 Prepare feature/property chains
		VkPhysicalDeviceDepthStencilResolveProperties resolveProps{};
		resolveProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
		accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

		VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
		rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		rayQueryFeatures.pNext = &accelFeatures;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
		rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rtPipelineFeatures.pNext = &rayQueryFeatures;

		VkPhysicalDeviceDescriptorIndexingFeatures indexingFeaturesCheck{};
		indexingFeaturesCheck.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		indexingFeaturesCheck.pNext = &rtPipelineFeatures;

		VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatureCheck{};
		bufferAddressFeatureCheck.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		bufferAddressFeatureCheck.pNext = &indexingFeaturesCheck;

		VkPhysicalDeviceVulkan11Features features11{};
		features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		features11.pNext = &bufferAddressFeatureCheck;

		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features13.pNext = &features11;

		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &features13;
		vkGetPhysicalDeviceFeatures2(device, &features2);

		VkPhysicalDeviceProperties2 props2{};
		props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		props2.pNext = &resolveProps;
		vkGetPhysicalDeviceProperties2(device, &props2);

		bool supported =
			deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			features2.features.geometryShader &&
			features2.features.samplerAnisotropy &&
			bufferAddressFeatureCheck.bufferDeviceAddress &&
			indexingFeaturesCheck.runtimeDescriptorArray &&
			indexingFeaturesCheck.shaderSampledImageArrayNonUniformIndexing &&
			indexingFeaturesCheck.descriptorBindingPartiallyBound &&
			indexingFeaturesCheck.descriptorBindingVariableDescriptorCount &&
			indexingFeaturesCheck.descriptorBindingSampledImageUpdateAfterBind &&
			accelFeatures.accelerationStructure &&
			rtPipelineFeatures.rayTracingPipeline &&
			rayQueryFeatures.rayQuery;
		if (!supported) {
			std::cerr << "Device is missing required features for descriptor indexing or buffer device address.\n";
		}
		return supported;
	}

	void Device::initQueueFamilies(VkPhysicalDevice device) {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			// 1 Find graphic queue
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				mGraphicQueueFamily = i;
			}
			// 2 Find present queue
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface->getSurface(), &presentSupport);
			if (presentSupport) {
				mPresentQueueFamily = i;
			}
			// 3 Find compute queue
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				mComputeQueueFamily = i;
			}
			if (isQueueFamilyComplete()) {
				break;
			}
			++i;
		}
	}

	void Device::createLogicalDevice() {
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> queueFamilies = { mGraphicQueueFamily.value(), mPresentQueueFamily.value(), mComputeQueueFamily.value() };
		float queuePriority = 1.0;

		// 1 Fill in queue family create info
		for (uint32_t queueFamily : queueFamilies) {

			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}
	
		// 2 Fill in device create info
		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
		accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		accelFeatures.accelerationStructure = VK_TRUE;
		accelFeatures.pNext = nullptr;

		VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
		rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
		rayQueryFeatures.rayQuery = VK_TRUE;
		rayQueryFeatures.pNext = &accelFeatures;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
		rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rtFeatures.rayTracingPipeline = VK_TRUE;
		rtFeatures.pNext = &rayQueryFeatures;

		VkPhysicalDeviceBufferDeviceAddressFeatures bda{};
		bda.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		bda.bufferDeviceAddress = VK_TRUE;
		bda.pNext = &rtFeatures;

		VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
		indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		indexingFeatures.runtimeDescriptorArray = VK_TRUE;
		indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		indexingFeatures.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
		indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
		indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
		indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		indexingFeatures.pNext = &bda;

		VkPhysicalDeviceVulkan11Features features11{};
		features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		features11.shaderDrawParameters = VK_TRUE;
		features11.pNext = &indexingFeatures;

		VkPhysicalDeviceVulkan13Features features13{};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features13.dynamicRendering = VK_TRUE;
		features13.pNext = &features11;

		VkPhysicalDeviceFeatures2 deviceFeatures{};
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures.features.shaderInt64 = VK_TRUE;
		deviceFeatures.features.samplerAnisotropy = VK_TRUE;
		deviceFeatures.features.multiDrawIndirect = VK_TRUE;
		deviceFeatures.features.fragmentStoresAndAtomics = VK_TRUE;
		deviceFeatures.pNext = &features13;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceRequiredExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceRequiredExtensions.data();
		deviceCreateInfo.pEnabledFeatures = nullptr;
		deviceCreateInfo.pNext = &deviceFeatures;

		// 3 Enable validation layer
		if (mInstance->getEnableValidationLayer()) {
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			deviceCreateInfo.enabledLayerCount = 0;
		}

		// 4 Create device
		if (vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to create logical device");
		}
		fpGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(mDevice, "vkGetBufferDeviceAddress"));
		if (!fpGetBufferDeviceAddress) {
			throw std::runtime_error("Error: failed to load vkGetBufferDeviceAddress");
		}
		// 5 Create queue
		vkGetDeviceQueue(mDevice, mGraphicQueueFamily.value(), 0, &mGraphicQueue);
		vkGetDeviceQueue(mDevice, mPresentQueueFamily.value(), 0, &mPresentQueue);
		vkGetDeviceQueue(mDevice, mComputeQueueFamily.value(), 0, &mComputeQueue);
	}

	PFN_vkGetBufferDeviceAddress Device::getBufferDeviceAddressFunction() const {
		return fpGetBufferDeviceAddress;
	}
	bool Device::isQueueFamilyComplete() {
		return mGraphicQueueFamily.has_value() && mPresentQueueFamily.has_value() && mComputeQueueFamily.has_value();
	}

	VkSampleCountFlagBits Device::getMaxUsableSampleCount() {	
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

		VkSampleCountFlags counts = std::min(
			props.limits.framebufferColorSampleCounts,
			props.limits.framebufferDepthSampleCounts
		);

		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}
}