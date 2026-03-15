#pragma once

#include "buffer.h"
#include "../../function/renderer/gpu_resource/texture/texture.h"
#include "../../function/renderer/gpu_resource/texture/cube_map_texture.h"

namespace flare::vk {

	struct UniformParameter {

		using Ptr = std::shared_ptr<UniformParameter>;
		static Ptr create() { return std::make_shared< UniformParameter>(); }

		size_t					mSize{ 0 };
		uint32_t				mBinding{ 0 };

		uint32_t				mCount{ 0 };
		VkDescriptorType		mDescriptorType;
		VkShaderStageFlags      mStage;

		std::vector<Buffer::Ptr> mBuffers{};
		flare::renderer::Texture::Ptr mTexture{ nullptr };
		flare::renderer::CubeMapTexture::Ptr mCubeMap{ nullptr };
		std::vector<flare::renderer::Texture::Ptr> mTextures{};

		std::vector<VkDescriptorImageInfo> mImageInfos{};
		std::vector<VkAccelerationStructureKHR> mAccelerationStructure{};
	};


	struct AttachmentDesc {
		
		VkFormat format;
		VkImageLayout imageLayout;
		VkSampleCountFlagBits samples;
		VkAttachmentLoadOp loadOp;
		VkAttachmentStoreOp storeOp;

		std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		float clearDepth = 1.0f;
		uint32_t clearStencil = 0;
	};

}
