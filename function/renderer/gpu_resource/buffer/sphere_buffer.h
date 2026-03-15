#pragma once
#include "../../../../common.h"
#include "../../../../platform/graphics/device.h"
#include "../../../../platform/graphics/buffer.h"
#include "../../../../platform/graphics/command_pool.h"
#include "../../../../platform/graphics/command_buffer.h"
#include "../../../loader/loader.h"

namespace flare::renderer {
	using namespace flare::vk;
	using namespace flare::loader;

    class SphereBuffer {
    public:
        using Ptr = std::shared_ptr<SphereBuffer>;
        static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const MeshData& meshData) {
            return std::make_shared<SphereBuffer>(device, commandPool, meshData);
        }

        SphereBuffer(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const MeshData& meshData);
        ~SphereBuffer() = default;

        std::vector<VkVertexInputBindingDescription> getVertexInputBindingDescriptions() {
            return {
                VkVertexInputBindingDescription{ 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX }
            };
        }

        std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
            return {
                VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }
            };
        }

        auto getVertexBuffer() const { return mVertexBuffer; }
        auto getIndexBuffer()  const { return mIndexBuffer; }
        auto getIndexCount()   const { return mIndexCount; }

    private:
        Buffer::Ptr    mVertexBuffer{ nullptr };
        Buffer::Ptr    mIndexBuffer{ nullptr };
        uint32_t mIndexCount = 0;
    };
}