#pragma once
#include "../../common.h"

namespace flare::vk {

    struct RayTracingContext {
        void init(VkPhysicalDevice physicalDevice, VkDevice device);

        uint32_t handleSize = 0;
        uint32_t handleAlignment = 0;
        uint32_t handleSizeAligned = 0;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
       
        PFN_vkGetBufferDeviceAddressKHR                vkGetBufferDeviceAddressKHR = nullptr;
        PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR = nullptr;
        PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR = nullptr;
        PFN_vkBuildAccelerationStructuresKHR           vkBuildAccelerationStructuresKHR = nullptr;
        PFN_vkCmdTraceRaysKHR                          vkCmdTraceRaysKHR = nullptr;
        PFN_vkGetRayTracingShaderGroupHandlesKHR       vkGetRayTracingShaderGroupHandlesKHR = nullptr;
        PFN_vkCreateRayTracingPipelinesKHR             vkCreateRayTracingPipelinesKHR = nullptr;
    };
}