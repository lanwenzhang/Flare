#include "acceleration_structure.h"

namespace flare::vk {

    Scratch AccelerationStructure::createScratch(VkDeviceSize size) {
        Scratch s{};
        s.buffer = createBufferDA(mDevice, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        s.addr = s.buffer->getDeviceAddress();
        return s;
    }

    void AccelerationStructure::createASStorageAndHandle(AccelerationStructureBuffer& as, VkAccelerationStructureTypeKHR type, const VkAccelerationStructureBuildSizesInfoKHR& sizeInfo) {

        // 1 create buffer
        as.storage = createBufferDA(mDevice, sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // 2 create acceleration structure
        VkAccelerationStructureCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        ci.buffer = as.storage->getBuffer();
        ci.size = sizeInfo.accelerationStructureSize;
        ci.type = type;
        mDevice->getRT().vkCreateAccelerationStructureKHR(mDevice->getDevice(), &ci, nullptr, &as.handle);

        // 3 get acceleration device address
        VkAccelerationStructureDeviceAddressInfoKHR ai{};
        ai.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        ai.accelerationStructure = as.handle;
        as.deviceAddress = mDevice->getRT().vkGetAccelerationStructureDeviceAddressKHR(mDevice->getDevice(), &ai);
    }

    void AccelerationStructure::buildASDevice(const VkAccelerationStructureBuildGeometryInfoKHR& buildInfo, const VkAccelerationStructureBuildRangeInfoKHR* const* pRanges) {

        auto pool = CommandPool::create(mDevice);
        auto cmd = CommandBuffer::create(mDevice, pool);

        cmd->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        mDevice->getRT().vkCmdBuildAccelerationStructuresKHR(cmd->getCommandBuffer(), 1, &buildInfo, pRanges);
        cmd->end();
        cmd->submitSync(mDevice->getGraphicQueue());
    }

    AccelerationStructureBuffer AccelerationStructure::buildBLAS(const TrianglesInput& g) {

        auto& rt = mDevice->getRT();

        // 1 geometry information
        VkDeviceOrHostAddressConstKHR vAddr{}, iAddr{}, tAddr{};
        vAddr.deviceAddress = g.vertex ? g.vertex->getDeviceAddress() : 0;
        iAddr.deviceAddress = g.index ? g.index->getDeviceAddress() : 0;
        tAddr.deviceAddress = g.transform ? g.transform->getDeviceAddress() : 0;

        VkAccelerationStructureGeometryKHR geo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geo.geometry.triangles = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            nullptr,
            g.vertexFormat,
            vAddr,
            g.vertexStride,
            g.vertexCount ? (g.vertexCount - 1) : 0,
            g.indexType,
            iAddr,
            tAddr
        };

        // 2 calculate dimension
        VkAccelerationStructureBuildGeometryInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        info.geometryCount = 1;
        info.pGeometries = &geo;

        const uint32_t primCount = (g.indexCount ? g.indexCount / 3 : g.vertexCount / 3);
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        rt.vkGetAccelerationStructureBuildSizesKHR(
            mDevice->getDevice(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &info, &primCount,
            &sizeInfo
        );

        // 3 create acceleration structure and handle
        AccelerationStructureBuffer as{};
        createASStorageAndHandle(as, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizeInfo);

        // 4 create scratch
        Scratch scratch = createScratch(sizeInfo.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR build{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.dstAccelerationStructure = as.handle;
        build.geometryCount = 1;
        build.pGeometries = &geo;
        build.scratchData.deviceAddress = scratch.addr;

        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = primCount;
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

        buildASDevice(build, &pRange);
        return as;
    }

    AccelerationStructureBuffer AccelerationStructure::buildTLASFromInstances(const std::vector<InstanceDesc>& instances){
        std::vector<VkAccelerationStructureInstanceKHR> cpu(instances.size());
        for (size_t i = 0; i < instances.size(); ++i) {
            const auto& s = instances[i];
            auto& d = cpu[i];
            d.transform = s.transform;
            d.instanceCustomIndex = s.customIndex;
            d.mask = s.mask;
            d.instanceShaderBindingTableRecordOffset = s.sbtRecordOffset;
            d.flags = s.flags;
            d.accelerationStructureReference = s.blas->deviceAddress;
        }
        auto instBuf = createBufferDA(mDevice, cpu.size() * sizeof(cpu[0]),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        instBuf->updateBufferByMap(cpu.data(), cpu.size() * sizeof(cpu[0]));

        VkDeviceOrHostAddressConstKHR instAddr{}; instAddr.deviceAddress = instBuf->getDeviceAddress();

        VkAccelerationStructureGeometryKHR geo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geo.geometry.instances = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            nullptr, VK_FALSE, instAddr
        };

        VkAccelerationStructureBuildGeometryInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        info.geometryCount = 1;
        info.pGeometries = &geo;

        uint32_t prims = (uint32_t)instances.size();
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        mDevice->getRT().vkGetAccelerationStructureBuildSizesKHR(mDevice->getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &prims, &sizeInfo);

        AccelerationStructureBuffer tlas{};
        createASStorageAndHandle(tlas, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizeInfo);
        Scratch scratch = createScratch(sizeInfo.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR build{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.dstAccelerationStructure = tlas.handle;
        build.geometryCount = 1;
        build.pGeometries = &geo;
        build.scratchData.deviceAddress = scratch.addr;

        VkAccelerationStructureBuildRangeInfoKHR range{}; range.primitiveCount = prims;
        const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
        buildASDevice(build, &pRange);

        return tlas;
    }

    std::vector<AccelerationStructureBuffer> AccelerationStructure::buildBLASForSubmeshes(const Buffer::Ptr& vbo, VkDeviceSize vertexStrideBytes, const Buffer::Ptr& ibo, const std::vector<SubmeshDesc>& subs,
                                                                                           VkFormat vtxFmt, VkIndexType indexType, bool markOpaque){
        std::vector<AccelerationStructureBuffer> out;
        out.reserve(subs.size());

        auto vAddrBase = vbo->getDeviceAddress();
        auto iAddrBase = ibo->getDeviceAddress();

        for (const auto& s : subs)
        {
            VkDeviceOrHostAddressConstKHR vAddr{}, iAddr{};
            vAddr.deviceAddress = vAddrBase + VkDeviceSize(s.vertexOffset) * vertexStrideBytes;
            iAddr.deviceAddress = iAddrBase + sizeof(uint32_t) * s.indexOffset;

            VkAccelerationStructureGeometryKHR geo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
            geo.flags = markOpaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : (VkGeometryFlagsKHR)0;
            geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geo.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, nullptr, vtxFmt, vAddr, vertexStrideBytes, s.maxVertex, indexType, iAddr, {}};

            VkAccelerationStructureBuildGeometryInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
            info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            info.geometryCount = 1;
            info.pGeometries = &geo;

            const uint32_t primCount = s.indexCount / 3;
            VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
            mDevice->getRT().vkGetAccelerationStructureBuildSizesKHR(mDevice->getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &primCount, &sizeInfo);

            AccelerationStructureBuffer as{};
            createASStorageAndHandle(as, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizeInfo);
            Scratch scratch = createScratch(sizeInfo.buildScratchSize);

            VkAccelerationStructureBuildGeometryInfoKHR build{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
            build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            build.dstAccelerationStructure = as.handle;
            build.geometryCount = 1;
            build.pGeometries = &geo;
            build.scratchData.deviceAddress = scratch.addr;

            VkAccelerationStructureBuildRangeInfoKHR range{};
            range.primitiveCount = primCount;
            const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

            buildASDevice(build, &pRange);
            out.push_back(as);
        }
        return out;
    }

    void AccelerationStructure::destroy(AccelerationStructureBuffer& as) {
        if (as.handle) {
            mDevice->getRT().vkDestroyAccelerationStructureKHR(mDevice->getDevice(), as.handle, nullptr);
            as.handle = VK_NULL_HANDLE;
        }
        as.deviceAddress = 0;
        as.storage.reset();
    }

}