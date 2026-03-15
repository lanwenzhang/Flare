#pragma once
#include "../../application/type.h"
#include "../../platform/graphics/device.h"
#include "../../platform/graphics/surface.h"
#include "../../platform/graphics/swap_chain.h"
#include "../../platform/graphics/command_pool.h"
#include "../../platform/graphics/command_buffer.h"
#include "../../platform/graphics/buffer.h"
#include "../../platform/graphics/descriptor_set_layout.h"
#include "../../platform/graphics/descriptor_pool.h"
#include "../../platform/graphics/description.h"
#include "../../platform/graphics/descriptor_set.h"
#include "../../platform/graphics/framebuffer.h"
#include "../../platform/graphics/image.h"
#include "../../platform/graphics/acceleration_structure.h"

#include "../scene/scene_graph.h"
#include "../scene/camera/camera.h"
#include "../scene/light/directional_light.h"
#include "../loader/loader.h"

#include "gpu_resource/buffer/geometry_buffer.h"
#include "gpu_resource/buffer/sphere_buffer.h"
#include "gpu_resource/texture/texture.h"
#include "gpu_resource/texture/cube_map_texture.h"
#include "gpu_resource/descriptor/frame_uniform_manager.h"
#include "gpu_resource/descriptor/static_uniform_manager.h"

#include "frame_graph.h"
#include "passes/depth_pre_pass.h"
#include "passes/depth_pyramid_pass.h"
#include "passes/culling_pass.h"
#include "passes/geometry_pass.h"
#include "passes/motion_vector_pass.h"
#include "passes/ray_traced_shadow_pass.h"
#include "passes/ddgi_pass.h"
#include "passes/gtao_pass.h"
#include "passes/lighting_pass.h"
#include "passes/taa_pass.h"
#include "passes/tone_mapping_pass.h"

#include "../profiler/profiler.h"

namespace flare::renderer {

	using namespace flare::vk;
	using namespace flare::loader;
	using namespace flare::scene;

	struct FrameGraphContext {
		CommandBuffer::Ptr cmd;
		uint32_t imageIndex = 0;
		int frameIndex = 0;
		Camera camera;
		DirectionalLight light;
		uint32_t numFrames;
	};

	class Renderer {
	public:
		using Ptr = std::shared_ptr<Renderer>;
		static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool) 
					     { return std::make_shared<Renderer>(device, commandPool); }
		Renderer(const Device::Ptr& device, const CommandPool::Ptr& commandPool) {  mDevice = device; mCommandPool = commandPool; }
		~Renderer();
		void createResources(int frameInFlight);
		void createDescriptorSets(int frameInFlight);
		void createPasses(const SwapChain::Ptr& swapChain, int frameInFlight);
		void createFrameGraph();

		// cmd
		void renderScene(const SwapChain::Ptr& swapChain, const CommandBuffer::Ptr& cmd, uint32_t imageIndex, int frameCount, int frameIndex, Camera camera, DirectionalLight light);
		void updateFrameUniform(int frameIndex, Camera camera) { mFrameUniformManager->update(camera.getViewMatrix(), camera.getJitteredProjectionMatrix(), mDescriptorSet_Frame, frameIndex);}
		void updateLightUniform(int frameIndex, flare::app::LightParam light) { mFrameUniformManager->updateLight(light, mDescriptorSet_Frame, frameIndex);  }
		void updateTAAUniform(int frameIndex, flare::app::TAAParams taa) { mFrameUniformManager->updateTAA(taa, mDescriptorSet_Frame, frameIndex);}
		
		// ui 
		void setDenoiseEnabled(bool e) { mEnableDenoise = e; }
		bool getDenoiseEnabled() const { return mEnableDenoise; }
		void setAODenoiseEnabled(bool e) { mEnableAODenoise = e; }
		bool getAODenoiseEnabled() const { return mEnableAODenoise; }
		void setTAAEnabled(bool e) { mEnableTAA = e; }
		bool getTAAEnabled() const { return mEnableTAA; }

		// profiler
		void setProfiler(const Profiler::Ptr& p) { mProfiler = p; }

	private:
		void saveHistory(const CommandBuffer::Ptr& cmd);

	public:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };

		Device::Ptr mDevice{ nullptr };
		CommandPool::Ptr mCommandPool{ nullptr };

		// global resources
		GeometryBuffer::Ptr mGeometryBuffer{ nullptr };
		SphereBuffer::Ptr mSphereBuffer{ nullptr };
		CubeMapTexture::Ptr mSkybox{ nullptr };

		FrameUniformManager::Ptr mFrameUniformManager{ nullptr };
		DescriptorSetLayout::Ptr mDescriptorSetLayout_Frame{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool_Frame{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet_Frame{ nullptr };

		StaticUniformManager::Ptr mStaticUniformManager{ nullptr };
		DescriptorSetLayout::Ptr mDescriptorSetLayout_Static{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool_Static{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet_Static{ nullptr };

		// passes
		bool mFrameGraphInit{ false };
		FrameGraph mFrameGraph;
		FrameGraphContext  mFrameGraphContext;

		DepthPrePass::Ptr mDepthPrePass{ nullptr };
		DepthPyramidPass::Ptr mDepthPyramidPass{ nullptr };
		CullingPass::Ptr mCullingPass{ nullptr };
		GeometryPass::Ptr mGeometryPass{ nullptr };
		MotionVectorPass::Ptr mMotionVectorPass{ nullptr };
		RayTracedShadowPass::Ptr mRayTracedShadowPass{ nullptr };
		DDGIPass::Ptr mDDGIPass{ nullptr };
		GTAOPass::Ptr mGTAOPass{ nullptr };
		LightingPass::Ptr mLightingPass{ nullptr };
		TAAPass::Ptr mTAAPass{ nullptr };
		ToneMappingPass::Ptr mToneMappingPass{ nullptr };

		// scene
		Scene    mScene;
		MeshData mSphereMeshData;
		MeshData mMeshData;
		AccelerationStructure::Ptr mAccelerationStructure{ nullptr };
		std::vector<AccelerationStructureBuffer> mBLAS{};
		AccelerationStructureBuffer mTLAS{};

		// profiler
		Profiler::Ptr mProfiler{ nullptr };

		// ui
		bool mEnableDenoise{ true };
		bool mEnableAODenoise{ true };
		bool mShowProbe{ false };
		bool mEnableTAA{ true };
		flare::app::LightPushConstant mLightPushConstants;
	};
}