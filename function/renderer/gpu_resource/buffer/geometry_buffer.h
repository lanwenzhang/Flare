#pragma once
#include "../../../../common.h"
#include "../../../../platform/graphics/device.h"
#include "../../../../platform/graphics/buffer.h"
#include "../../../../platform/graphics/command_pool.h"
#include "../../../../platform/graphics/command_buffer.h"
#include "../../../scene/scene_graph.h"
#include "../../../loader/loader.h"
#include "../texture/texture.h"

namespace flare::renderer {

    using namespace flare::vk;
    using namespace flare::loader;

    class GeometryBuffer {
    public:
        using Ptr = std::shared_ptr<GeometryBuffer>;
        static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, MeshData& meshData, Scene& scene, int frameCount) {
            return std::make_shared<GeometryBuffer>(device, commandPool, meshData, scene, frameCount);
        }

        GeometryBuffer(const Device::Ptr& device, const CommandPool::Ptr& commandPool, MeshData& meshData, Scene& scene,int frameCount);
        ~GeometryBuffer();
        void draw(const CommandBuffer::Ptr& cmd);

        std::vector<VkVertexInputBindingDescription> getVertexInputBindingDescriptions() {
            return { VkVertexInputBindingDescription{ 0, sizeof(float) * 12, VK_VERTEX_INPUT_RATE_VERTEX } };
        }

        std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
            return {
                { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
                { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3 },
                { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 5 },
                { 3, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 8 }
            };
        }

        [[nodiscard]] auto getVertexBuffer() const { return mVertexBuffer; }
        [[nodiscard]] auto getIndexBuffer() const { return mIndexBuffer; }
        [[nodiscard]] auto getIndirectBuffer() const { return mIndirectBuffer; }
        [[nodiscard]] auto getDrawCount() const { return mDrawCount; }
        [[nodiscard]] auto getSphereBuffer() const { return mSphereBuffer; }
    private:

        Device::Ptr mDevice{ nullptr };
        CommandPool::Ptr mCommandPool{ nullptr };
        Buffer::Ptr mVertexBuffer{ nullptr };
        Buffer::Ptr mIndexBuffer{ nullptr };
        Buffer::Ptr mIndirectBuffer{ nullptr };
        Buffer::Ptr mSphereBuffer{ nullptr };

        uint32_t mDrawCount{ 0 };
    };
}

