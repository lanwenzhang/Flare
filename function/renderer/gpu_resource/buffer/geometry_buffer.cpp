#include "geometry_buffer.h"

namespace flare::renderer {

    GeometryBuffer::GeometryBuffer(const Device::Ptr& device, const CommandPool::Ptr& commandPool, MeshData& meshData, Scene& scene,int frameCount) {

        mDevice = device;
        mCommandPool = commandPool;
        mVertexBuffer = Buffer::createVertexBuffer(mDevice, meshData.vertexData.size(), meshData.vertexData.data(), true);
        mIndexBuffer = Buffer::createIndexBuffer(mDevice, meshData.indexData.size() * sizeof(uint32_t), meshData.indexData.data(), true);

        std::vector<VkDrawIndexedIndirectCommand> drawCommands;
        drawCommands.reserve(scene.drawDataArray.size());
        for (size_t i = 0; i < scene.drawDataArray.size(); ++i) {
            const auto& dd = scene.drawDataArray[i];
            const auto& mesh = meshData.meshes[i];

            VkDrawIndexedIndirectCommand cmd{};
            cmd.indexCount = mesh.indexCount;
            cmd.instanceCount = 1;
            cmd.firstIndex = mesh.indexOffset;
            cmd.vertexOffset = mesh.vertexOffset;      
            cmd.firstInstance = static_cast<uint32_t>(i);

            drawCommands.push_back(cmd);
        }


        mDrawCount = static_cast<uint32_t>(drawCommands.size());
        mIndirectBuffer = Buffer::createStorageBuffer(mDevice, drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand), drawCommands.data(), false);

        // sphere buffer
        std::vector<BoundingSphereGPU> reordered;
        reordered.reserve(mDrawCount);
        for (size_t i = 0; i < scene.drawDataArray.size(); ++i) {
            const auto& dd = scene.drawDataArray[i];
            const auto& sphereL = meshData.spheres[i];
            const glm::mat4& m = scene.globalTransform[dd.transformId];
            const glm::vec3 centerL = glm::vec3(sphereL);
            const float radiusL = sphereL.w;
            reordered.push_back(toSphereGPU(centerL, radiusL, m));
        }

        mSphereBuffer = flare::vk::Buffer::createStorageBuffer(mDevice, reordered.size() * sizeof(BoundingSphereGPU), reordered.data(), false);
    }

    GeometryBuffer::~GeometryBuffer() {}

    void GeometryBuffer::draw(const CommandBuffer::Ptr& cmd) {
        cmd->bindVertexBuffer({ mVertexBuffer->getBuffer() });
        cmd->bindIndexBuffer(mIndexBuffer->getBuffer());
        cmd->drawIndexedIndirect(mIndirectBuffer->getBuffer(), 0, mDrawCount, sizeof(VkDrawIndexedIndirectCommand));
    }

}


