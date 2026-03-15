#include "sphere_buffer.h"

namespace flare::renderer {
	SphereBuffer::SphereBuffer(const Device::Ptr& device, const CommandPool::Ptr& commandPool, const MeshData& meshData) {
		mVertexBuffer = Buffer::createVertexBuffer(device, meshData.vertexData.size(), meshData.vertexData.data(), true);
		mIndexBuffer = Buffer::createIndexBuffer(device, meshData.indexData.size() * sizeof(uint32_t), meshData.indexData.data(), true);
		mIndexCount = meshData.indexData.size();

	}
}