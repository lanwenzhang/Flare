#pragma once
#include "../../../common.h"
#include "../../../platform/graphics/device.h"
#include "../../../platform/graphics/command_pool.h"
#include "../../../platform/graphics/command_buffer.h"
#include "../../../platform/graphics/ray_tracing_pipeline.h"
#include "../../../platform/graphics/compute_pipeline.h"
#include "../../../platform/graphics/descriptor_set.h"
#include "../../../platform/graphics/image.h"
#include "../../loader/loader.h"
#include "../gpu_resource/buffer/geometry_buffer.h"
#include "../gpu_resource/texture/texture.h"
#include "../../scene/camera/camera.h"
#include "../frame_graph.h"
#include "geometry_pass.h"

namespace flare::renderer {
	using namespace flare::vk;

	#define PROBE_STATUS_OFF 0
	#define PROBE_STATUS_SLEEP 1
	#define PROBE_STATUS_ACTIVE 4
	#define PROBE_STATUS_UNINITIALIZED 6

	struct SubmeshGpu {
		uint32_t indexOffset;
		uint32_t vertexOffset;
		uint32_t materialId;
		uint32_t pad0;
	};

	struct DDGIParams {
		glm::vec4 probeGridPosition = glm::vec4(-10.0f, 0.5f, -10.0f, 0.0f);
		glm::vec4 probeSpacing = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f); // spacing and scale
		glm::vec4 reciprocalProbeSpacing = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		glm::ivec4 probeCounts = glm::ivec4(20, 12, 20, 0); // x, y, z, total

		int   irradianceWidth;
		int   irradianceHeight;
		int   irradianceSideLength = 6;
		int   probeRays = 128;

		float selfShadowBias = 0.3f;
		float hysteresis = 0.95f;
		float infiniteBounceMultiplier = 0.75f;
		float maxOffset = 0.4f;

		uint32_t enableInfiniteBounce = 1u;
		uint32_t enableBackfaceBlending = 1u;
		uint32_t enableSmoothBackface = 1u;
		uint32_t enableProbeOffset = 1u;

		glm::mat4 randomRotation = glm::mat4(1.0f);
	};

	class DDGIPass {
	public:
		using Ptr = std::shared_ptr<DDGIPass>;
		static Ptr create(Device::Ptr& device) { return std::make_shared<DDGIPass>(device); }
		DDGIPass(Device::Ptr& device) { mDevice = device; }
		~DDGIPass() = default;

		void init(const CommandPool::Ptr& commandPool, const flare::loader::MeshData& meshData, const DescriptorSetLayout::Ptr& frameSetLayout,
				  const DescriptorSetLayout::Ptr& staticSetLayout, const FrameGraph& frameGraph, const GeometryPass::Ptr& geometryPass,
				  const CubeMapTexture::Ptr& skybox, int frameCount);
		void render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, 
					int frameIndex, Camera camera);

		[[nodiscard]] auto getProbeRTDS() { return mDescriptorSet; }
		[[nodiscard]] auto getProbeRTDSLayout() { return mDescriptorSetLayout; }
		[[nodiscard]] auto getProbeIrradiance() const{ return mProbeIrradiance[mIrradiancePingPongIndex];}
		[[nodiscard]] auto getPrevIrradiance() { return mPrevIrradiance; }
		[[nodiscard]] auto getProbeVisibility() const { return mVisibility[mVisibilityPingPongIndex]; }
		[[nodiscard]] auto getPrevVisibility() { return mPrevVisibility; }
		[[nodiscard]] auto getProbeOffset() const { return mOffset[mOffsetPingPongIndex]; }
		[[nodiscard]] auto getPrevOffset() { return mPrevOffset; }
		[[nodiscard]] auto getIndirectLightingTex() { return mIndirectLighting; }

	private:
		void createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph);
		void createProbeRTDS(const flare::loader::MeshData& meshData);
		void createOffsetDS();
		void createIrradianceDS();
		void createVisibilityDS();
		void createSampleIrradianceDS();
		void createPipelines(const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout);
		void probeRayTrace(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex, Camera camera);
		void updateProbeOffset(const CommandBuffer::Ptr& cmd, int frameIndex);
		void updateProbeStatus(const CommandBuffer::Ptr& cmd, int frameIndex);
		void updateIrradiance(const CommandBuffer::Ptr& cmd, int frameIndex);
		void updateVisibility(const CommandBuffer::Ptr& cmd, int frameIndex);
		void sampleIrradiance(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, int frameIndex, Camera camera);
		void updateBuffer(const DescriptorSet::Ptr& descriptorSet, int frameIndex);
		float getRandomValue(float min, float max);

	public:
		DDGIParams mDDGI;

	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		int mFrameCount{ 0 };
		Device::Ptr mDevice{ nullptr };

		// probe rt pass
		RayTracingPipeline::Ptr mProbeRTPipeline{ nullptr };
		Texture::Ptr mRayTraceRadiance{ nullptr };
		CubeMapTexture::Ptr mSkybox{ nullptr };
		UniformParameter::Ptr mDDGIParam{ nullptr };
		UniformParameter::Ptr mRadianceParam{ nullptr };
		UniformParameter::Ptr mProbeStatusParam{ nullptr };
		DescriptorSetLayout::Ptr mDescriptorSetLayout{ nullptr };
		DescriptorPool::Ptr      mDescriptorPool{ nullptr };
		DescriptorSet::Ptr       mDescriptorSet{ nullptr };

		// offset pass
		ComputePipeline::Ptr mOffsetPipeline{ nullptr };
		Texture::Ptr mOffset[2]{ nullptr, nullptr };
		Texture::Ptr mPrevOffset{ nullptr };
		uint32_t     mOffsetPingPongIndex = 0;
		bool mFirstOffsetFrame{ false };
		UniformParameter::Ptr    mRadianceForOffsetParam{ nullptr };
		UniformParameter::Ptr    mOffsetWriteParam{ nullptr };
		UniformParameter::Ptr    mOffsetReadParam{ nullptr };
		DescriptorSetLayout::Ptr mOffsetSetLayout{ nullptr };
		DescriptorPool::Ptr      mOffsetPool{ nullptr };
		DescriptorSet::Ptr       mOffsetSet{ nullptr };

		// probe status pass
		ComputePipeline::Ptr mProbeStatusPipeline{ nullptr };
		bool mProbeStatusFirstFrame{ false };

		// irradiance pass
		ComputePipeline::Ptr mIrradiancePipeline{ nullptr };
		Texture::Ptr mProbeIrradiance[2]{ nullptr, nullptr };
		Texture::Ptr mPrevIrradiance{ nullptr };
		uint32_t     mIrradiancePingPongIndex = 0;
		UniformParameter::Ptr    mRadianceForIrrParam{ nullptr };
		UniformParameter::Ptr    mIrradianceWriteParam{ nullptr };
		UniformParameter::Ptr    mIrradianceReadParam{ nullptr };
		DescriptorSetLayout::Ptr mIrradianceSetLayout{ nullptr };
		DescriptorPool::Ptr      mIrradiancePool{ nullptr };
		DescriptorSet::Ptr       mIrradianceSet{ nullptr };

		// visibility pass
		ComputePipeline::Ptr mVisibilityPipeline{ nullptr };
		Texture::Ptr mVisibility[2]{ nullptr, nullptr };
		Texture::Ptr mPrevVisibility{ nullptr };
		uint32_t mVisibilityPingPongIndex = 0;
		UniformParameter::Ptr    mRadianceForVisParam{ nullptr };
		UniformParameter::Ptr    mVisibilityWriteParam{ nullptr };
		UniformParameter::Ptr    mVisibilityReadParam{ nullptr };
		DescriptorSetLayout::Ptr mVisibilitySetLayout{ nullptr };
		DescriptorPool::Ptr      mVisibilityPool{ nullptr };
		DescriptorSet::Ptr       mVisibilitySet{ nullptr };

		// sample irradiance pass
		ComputePipeline::Ptr mSamplePipeline{ nullptr };
		Texture::Ptr mIndirectLighting{ nullptr };
		Texture::Ptr mNormal{ nullptr };
		Texture::Ptr mDepth{ nullptr };
		UniformParameter::Ptr    mNormalParam{ nullptr };
		UniformParameter::Ptr    mDepthParam{ nullptr };
		UniformParameter::Ptr    mSampleIrradianceParam{ nullptr };
		UniformParameter::Ptr    mSampleVisibilityParam{ nullptr };
		UniformParameter::Ptr    mIndirectParam{ nullptr };
		DescriptorSetLayout::Ptr mSampleSetLayout{ nullptr };
		DescriptorPool::Ptr      mSamplePool{ nullptr };
		DescriptorSet::Ptr       mSampleSet{ nullptr };
	};
}