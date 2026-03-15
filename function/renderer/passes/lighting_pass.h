#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/swap_chain.h"
#include "../../../platform/graphics/command_pool.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../frame_graph.h"
#include "../gpu_resource/buffer/sphere_buffer.h"
#include "../gpu_resource/texture/texture.h"
#include "../gpu_resource/texture/cube_map_texture.h"
#include "../../scene/camera/camera.h"
#include "../../scene/light/directional_light.h"
#include "geometry_pass.h"
#include "ddgi_pass.h"
#include "gtao_pass.h"

namespace flare::renderer {

	using namespace flare::vk;
	class LightingPass {
	public:
		using Ptr = std::shared_ptr<LightingPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<LightingPass>(device); }

		LightingPass(Device::Ptr& device) { mDevice = device; }
		~LightingPass() = default;

		void init(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const DescriptorSetLayout::Ptr& frameSetLayout,
				  const GeometryPass::Ptr& geometryPass, const DDGIPass::Ptr& ddgiPass,
				  const Texture::Ptr& visibilityTex, const Texture::Ptr& indirectTex, const GTAOPass::Ptr& gtaoPass,
				  const CubeMapTexture::Ptr& skybox, const SphereBuffer::Ptr& sphereBuffer,int frameCount);
		void setShadowTexture(const Texture::Ptr& tex, int frameIndex);
		void setAOTexture(const Texture::Ptr& tex, int frameIndex);
		void setProbeIrradianceTexture(const Texture::Ptr& tex, int frameIndex);
		void render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& probeSet,
					const SphereBuffer::Ptr& sphereBuffer, int probeCount,
				    Camera camera, DirectionalLight light, flare::app::LightPushConstant pc, int frameIndex);
		[[nodiscard]] auto getSkybox() const { return mSkybox; }
		[[nodiscard]] auto getLightingTex() const { return mLighting; }

	private:
		void createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph, const GeometryPass::Ptr& geometryPass);
		void createPipeline(const FrameGraph& frameGraph, const DescriptorSetLayout::Ptr& frameSetLayout, 
							const DescriptorSetLayout::Ptr& probeSetLayout, const SphereBuffer::Ptr& sphereBuffer);
		void createDescriptorSet();
		void createSphereDS();

	public:
		bool mShowProbe{ false };

	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		int mFrameCount{ 0 };

		Device::Ptr mDevice{ nullptr };
		Pipeline::Ptr mPipeline{ nullptr };
		Pipeline::Ptr mSpherePipeline{ nullptr };
		Pipeline::Ptr mSkyboxPipeline{ nullptr };
		std::vector<UniformParameter::Ptr> mParams{};
		DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet{ nullptr };

		Framebuffer::Ptr mLightingFb{ nullptr };
		Texture::Ptr mLighting{ nullptr };

		Texture::Ptr mColor{ nullptr };
		Texture::Ptr mNormal{ nullptr };
		Texture::Ptr mDepth{ nullptr };
		Texture::Ptr mMR{ nullptr };
		Texture::Ptr mVisibility{ nullptr };
		Texture::Ptr mShadowTex{ nullptr };
		Texture::Ptr mIndirect{ nullptr };
		Texture::Ptr mAORaw{ nullptr };
		Texture::Ptr mAOTex{ nullptr };
		CubeMapTexture::Ptr mSkybox{ nullptr };
		CubeMapTexture::Ptr mIrradianceMap{ nullptr };

		Texture::Ptr mProbeIrradiance{ nullptr };
		Texture::Ptr mProbeOffset{ nullptr };
		std::vector<UniformParameter::Ptr> mSphereParams;
		DescriptorSetLayout::Ptr mSphereDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mSphereDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mSphereDescriptorSet{ nullptr };

	};
}