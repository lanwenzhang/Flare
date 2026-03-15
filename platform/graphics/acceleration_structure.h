#pragma once
#include "../../common.h"
#include "device.h"
#include "buffer.h"
#include "command_pool.h"
#include "command_buffer.h"  
#include "ray_tracing_context.h"

namespace flare::vk {
	
    struct AccelerationStructureBuffer {
        VkAccelerationStructureKHR handle { VK_NULL_HANDLE };
        uint64_t                   deviceAddress{ 0 };
        Buffer::Ptr                storage{ nullptr };
    };

    struct TrianglesInput {
        Buffer::Ptr  vertex{ nullptr };
        VkDeviceSize vertexStride = 0;
        uint32_t     vertexCount = 0;
        VkFormat     vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        Buffer::Ptr  index{ nullptr };
        uint32_t     indexCount = 0;
        VkIndexType  indexType = VK_INDEX_TYPE_UINT32;
        Buffer::Ptr transform{ nullptr };
    };

    struct Scratch {
        Buffer::Ptr buffer{ nullptr };
        uint64_t    addr{ 0 };
    };

    struct SubmeshDesc {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;  
        uint32_t vertexOffset = 0; 
        uint32_t maxVertex = 0;
    };

    struct InstanceDesc {
        const AccelerationStructureBuffer* blas = nullptr;
        VkTransformMatrixKHR transform{};
        uint32_t customIndex = 0;
        uint8_t  mask = 0xFF;
        uint8_t  sbtRecordOffset = 0;
        VkGeometryInstanceFlagsKHR flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    };

    class AccelerationStructure {
    public:
        using Ptr = std::shared_ptr<AccelerationStructure>;
        static Ptr create(Device::Ptr device) { return std::make_shared<AccelerationStructure>(device); }
        AccelerationStructure(Device::Ptr device) { mDevice = device; }
        ~AccelerationStructure() = default;

        AccelerationStructureBuffer buildBLAS(const TrianglesInput& g);
        std::vector<AccelerationStructureBuffer> buildBLASForSubmeshes(const Buffer::Ptr& sharedVertexBuffer, VkDeviceSize vertexStrideBytes, const Buffer::Ptr& sharedIndexBuffer,
                                                                        const std::vector<SubmeshDesc>& submeshes, VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, 
                                                                        VkIndexType indexType = VK_INDEX_TYPE_UINT32, bool markOpaque = true);
        AccelerationStructureBuffer buildTLASFromInstances(const std::vector<InstanceDesc>& instances);
        void destroy(AccelerationStructureBuffer& as);

    private:


        static Buffer::Ptr createBufferDA(const Device::Ptr& dev, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
            return Buffer::create(dev, size, usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, props, true);
        }

        Scratch createScratch(VkDeviceSize size);
        void createASStorageAndHandle(AccelerationStructureBuffer& as, VkAccelerationStructureTypeKHR type, const VkAccelerationStructureBuildSizesInfoKHR& sizeInfo);
        void buildASDevice(const VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const VkAccelerationStructureBuildRangeInfoKHR* const* pRanges);

    private:
        Device::Ptr mDevice{ nullptr };
    };

}