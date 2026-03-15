#include "renderer.h"

namespace flare::renderer {

	Renderer::~Renderer() {
		if (mAccelerationStructure){
			if (mTLAS.handle != VK_NULL_HANDLE) {
				mAccelerationStructure->destroy(mTLAS);
			}
			for (auto& b : mBLAS) {
				if (b.handle != VK_NULL_HANDLE) {
					mAccelerationStructure->destroy(b);
				}
			}
		}
	}

	void Renderer::createResources(int frameInFlight) {
		// 1 texture 
		if (mFrameGraphInit) return;
		mFrameGraph.clear();
		mFrameGraph.init(mDevice, mCommandPool);
		mFrameGraph.loadFromJsonFile("frame_graph/frame_graph.json");
		mFrameGraph.createImages();

		mSkybox = CubeMapTexture::create(mDevice, mCommandPool, "assets/skybox/immenstadter_horn_2k_prefilter.ktx");
		auto cmd = CommandBuffer::create(mDevice, mCommandPool);
		cmd->begin();
		cmd->transitionImageLayout(mSkybox->getImage()->getImage(), mSkybox->getImage()->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, 0, 6);
		cmd->end();
		cmd->submitSync(mDevice->getGraphicQueue());

		// 2 buffers
		flare::loader::loadobj("assets/sphere/sphere.obj", mSphereMeshData);
		flare::loader::loadgltf("assets/Sponza/glTF/Sponza.gltf", mMeshData, mScene);
		mSphereBuffer = SphereBuffer::create(mDevice, mCommandPool, mSphereMeshData);
		mGeometryBuffer = GeometryBuffer::create(mDevice, mCommandPool, mMeshData, mScene, frameInFlight);

		// 3 acceleration structure
		mAccelerationStructure = AccelerationStructure::create(mDevice);
		auto vertexBuffer = mGeometryBuffer->getVertexBuffer();
		auto indexBuffer = mGeometryBuffer->getIndexBuffer();
		VkDeviceSize vertexStride = sizeof(float) * 12;

		// 3.1 sub mesh description
		std::vector<SubmeshDesc> submeshes;
		submeshes.reserve(mMeshData.meshes.size());

		for (const auto& mesh : mMeshData.meshes){
			SubmeshDesc s{};
			s.indexOffset = mesh.indexOffset;
			s.indexCount = mesh.indexCount;
			s.vertexOffset = mesh.vertexOffset;

			uint32_t maxIdx = 0;
			for (uint32_t i = 0; i < mesh.indexCount; ++i){
				uint32_t idx = mMeshData.indexData[mesh.indexOffset + i];
				if (idx > maxIdx) maxIdx = idx;
			}
			s.maxVertex = maxIdx;
			submeshes.push_back(s);
		}
		// 3.2 blas
		mBLAS = mAccelerationStructure->buildBLASForSubmeshes(vertexBuffer, vertexStride, indexBuffer, submeshes, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_UINT32, true);

		// 3.3 instances
		std::vector<InstanceDesc> instances;
		instances.reserve(mBLAS.size());

		for (size_t i = 0; i < mBLAS.size(); ++i){
			InstanceDesc inst{};
			inst.blas = &mBLAS[i];

			uint32_t transformId = mScene.drawDataArray[static_cast<uint32_t>(i)].transformId;
			const glm::mat4& M = mScene.globalTransform[transformId];
			VkTransformMatrixKHR t{};
			for (int r = 0; r < 3; ++r)
				for (int c = 0; c < 4; ++c)
					t.matrix[r][c] = M[c][r];

			inst.transform = t;
			inst.customIndex = static_cast<uint32_t>(i);
			inst.mask = 0xFF;
			inst.sbtRecordOffset = 0;
			inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instances.push_back(inst);
		}

		// 3.4 tlas
		mTLAS = mAccelerationStructure->buildTLASFromInstances(instances);
	}

	void Renderer::createDescriptorSets(int frameInFlight){
		// per frame
		mFrameUniformManager = flare::renderer::FrameUniformManager::create();
		mFrameUniformManager->init(mDevice, frameInFlight);
		auto frameParams = mFrameUniformManager->getParams();

		mDescriptorSetLayout_Frame = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout_Frame->build(frameParams);
		mDescriptorPool_Frame = DescriptorPool::create(mDevice);
		mDescriptorPool_Frame->build(frameParams, frameInFlight);
		mDescriptorSet_Frame = DescriptorSet::create(mDevice, frameParams, mDescriptorSetLayout_Frame, mDescriptorPool_Frame, frameInFlight);

		// per batch
		mStaticUniformManager = flare::renderer::StaticUniformManager::create(mDevice, mCommandPool, mScene, mMeshData, mGeometryBuffer, mTLAS, 1);
		auto staticParams = mStaticUniformManager->getParams();

		mDescriptorSetLayout_Static = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout_Static->build(staticParams);
		mDescriptorPool_Static = DescriptorPool::create(mDevice);
		mDescriptorPool_Static->build(staticParams, 1);
		mDescriptorSet_Static = DescriptorSet::create(mDevice, staticParams, mDescriptorSetLayout_Static, mDescriptorPool_Static, 1);
	}

	void Renderer::createPasses(const SwapChain::Ptr& swapChain, int frameInFlight) {
		mDepthPrePass = DepthPrePass::create(mDevice);
		mDepthPrePass->init(mFrameGraph, mGeometryBuffer, mDescriptorSetLayout_Frame, mDescriptorSetLayout_Static);

		mDepthPyramidPass = DepthPyramidPass::create(mDevice);
		mDepthPyramidPass->init(mFrameGraph, mDepthPrePass->getDepthPreLinear(), frameInFlight);

		mCullingPass = CullingPass::create(mDevice);
		mCullingPass->init(mDescriptorSetLayout_Static, mDepthPyramidPass->getHZB(), frameInFlight);

		mGeometryPass = GeometryPass::create(mDevice);
		mGeometryPass->init(mCommandPool, mFrameGraph, mGeometryBuffer, mDescriptorSetLayout_Frame, mDescriptorSetLayout_Static);

		mMotionVectorPass = MotionVectorPass::create(mDevice);
		mMotionVectorPass->init(mGeometryPass->getGbufferDepth(), mFrameGraph, mDescriptorSetLayout_Frame, frameInFlight);

		mRayTracedShadowPass = RayTracedShadowPass::create(mDevice);
		mRayTracedShadowPass->init(mCommandPool, mFrameGraph, mGeometryPass, mMotionVectorPass,
								   mDescriptorSetLayout_Frame, mDescriptorSetLayout_Static, frameInFlight);

		mDDGIPass = DDGIPass::create(mDevice);
		mDDGIPass->init(mCommandPool, mMeshData, mDescriptorSetLayout_Frame, mDescriptorSetLayout_Static,
						mFrameGraph, mGeometryPass,mSkybox, frameInFlight);

		mGTAOPass = GTAOPass::create(mDevice);
		mGTAOPass->init(mCommandPool, mGeometryPass, mDescriptorSetLayout_Frame, frameInFlight);

		mLightingPass = LightingPass::create(mDevice);
		mLightingPass->init(mCommandPool, mFrameGraph, mDescriptorSetLayout_Frame, mGeometryPass, mDDGIPass,
							mRayTracedShadowPass->getVisibility(), mDDGIPass->getIndirectLightingTex(), mGTAOPass,
							mSkybox, mSphereBuffer, frameInFlight);

		mTAAPass = TAAPass::create(mDevice);
		mTAAPass->init(mCommandPool, mFrameGraph, mGeometryPass->getGbufferDepth(), mLightingPass->getLightingTex(),
					   mMotionVectorPass->getMotionVectors(), mDescriptorSetLayout_Frame, frameInFlight);

		mToneMappingPass = ToneMappingPass::create(mDevice);
		mToneMappingPass->init(swapChain, mLightingPass->getLightingTex(), frameInFlight);
	}

	void Renderer::createFrameGraph() {
		mFrameGraph.setRecord("depth_pre_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::DepthPrePass, mFrameGraphContext.frameIndex);
			mDepthPrePass->render(mFrameGraphContext.cmd, mGeometryBuffer, mDescriptorSet_Frame, mDescriptorSet_Static,
							      mFrameGraphContext.camera, mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("depth_pyramid_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::DepthPyramidPass, mFrameGraphContext.frameIndex);
			mDepthPyramidPass->render(mFrameGraphContext.cmd, mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("culling_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::CullingPass, mFrameGraphContext.frameIndex);
			mCullingPass->render(mFrameGraphContext.cmd, mGeometryBuffer, mDescriptorSet_Static, mFrameGraphContext.camera, 
				                 mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("gbuffer_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::GBufferPass, mFrameGraphContext.frameIndex);
			mGeometryPass->render(mFrameGraphContext.cmd, mGeometryBuffer, mDescriptorSet_Frame, mDescriptorSet_Static, 
				                  mFrameGraphContext.camera, mFrameGraphContext.light, mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("motion_vector_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::MotionVectorPass, mFrameGraphContext.frameIndex);
			mMotionVectorPass->render(mFrameGraphContext.cmd, mDescriptorSet_Frame, mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("ray_traced_shadow_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::RayTracedShadowPass, mFrameGraphContext.frameIndex);
			mRayTracedShadowPass->render(mFrameGraphContext.cmd, mDescriptorSet_Frame, mDescriptorSet_Static,mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("ddgi_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::DDGIPass, mFrameGraphContext.frameIndex);
			mDDGIPass->render(mFrameGraphContext.cmd, mDescriptorSet_Frame, mDescriptorSet_Static, mFrameGraphContext.frameIndex, mFrameGraphContext.camera);
		});
		mFrameGraph.setRecord("gtao_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::GTAOPass, mFrameGraphContext.frameIndex);
			mGTAOPass->render(mFrameGraphContext.cmd, mDescriptorSet_Frame, mFrameGraphContext.camera, mFrameGraphContext.frameIndex);
			});
		mFrameGraph.setRecord("lighting_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::LightingPass, mFrameGraphContext.frameIndex);
			mLightingPass->setShadowTexture(mEnableDenoise ? mRayTracedShadowPass->getFilteredVisibility(): mRayTracedShadowPass->getVisibility(), mFrameGraphContext.frameIndex);
			mLightingPass->setProbeIrradianceTexture(mDDGIPass->getProbeIrradiance(), mFrameGraphContext.frameIndex);
			mLightingPass->setAOTexture(mEnableAODenoise ? mGTAOPass->getFilteredAO() : mGTAOPass->getAO(), mFrameGraphContext.frameIndex);
			mLightingPass->render(mFrameGraphContext.cmd, mDescriptorSet_Frame, mDDGIPass->getProbeRTDS(),
								  mSphereBuffer, mDDGIPass->mDDGI.probeCounts.w,
								  mFrameGraphContext.camera,mFrameGraphContext.light, mLightPushConstants, mFrameGraphContext.frameIndex);
		});
		mFrameGraph.setRecord("taa_pass", [this]() {
			ProfilerScope scope(mProfiler.get(), mFrameGraphContext.cmd, ProfilerSection::TAAPass, mFrameGraphContext.frameIndex);
			mTAAPass->render(mFrameGraphContext.cmd, mLightingPass->getLightingTex(), mDescriptorSet_Frame, mFrameGraphContext.frameIndex);
		});
		if (!mFrameGraph.compile()) {
			std::fprintf(stderr, "[Renderer] ERROR: FrameGraph compile failed.\n");
			return;
		}
		mFrameGraphInit = true;
	}

	void Renderer::renderScene(const SwapChain::Ptr& swapChain, const CommandBuffer::Ptr& cmd, uint32_t imageIndex, int frameCount, int frameIndex, Camera camera, DirectionalLight light) {
		if (!mFrameGraphInit) {
			createFrameGraph();
		}
		mFrameGraphContext.cmd = cmd;
		mFrameGraphContext.imageIndex = imageIndex;
		mFrameGraphContext.frameIndex = frameIndex;
		mFrameGraphContext.camera = camera;
		mFrameGraphContext.light = light;
		mFrameGraph.execute();
		{
			ProfilerScope scope(mProfiler.get(), cmd, ProfilerSection::ToneMappingPass, frameIndex);
			mToneMappingPass->render(cmd, swapChain, mEnableTAA ? mTAAPass->getCurrentHistoryTexture() : mLightingPass->getLightingTex(), imageIndex, frameIndex);
		}
		saveHistory(cmd);
	}

	void Renderer::saveHistory(const CommandBuffer::Ptr& cmd) {

		auto copyLastFrameTex = [&](const Texture::Ptr& src, const Texture::Ptr& dst, VkImageAspectFlags aspect, VkImageLayout srcFinalLayout)
			{
				auto srcImg = src->getImage();
				auto dstImg = dst->getImage();

				const uint32_t w = srcImg->getWidth();
				const uint32_t h = srcImg->getHeight();

				cmd->transitionImageLayout(srcImg->getImage(), srcImg->getFormat(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				cmd->transitionImageLayout(dstImg->getImage(), dstImg->getFormat(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				cmd->copyImage(srcImg->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					dstImg->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					w, h, aspect);

				cmd->transitionImageLayout(srcImg->getImage(), srcImg->getFormat(),
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcFinalLayout,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

				cmd->transitionImageLayout(dstImg->getImage(), dstImg->getFormat(),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
			};

		copyLastFrameTex(mGeometryPass->getGbufferNormal(), mGeometryPass->getPrevGbufferNormal(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		copyLastFrameTex(mRayTracedShadowPass->getFilteredVisibility(), mRayTracedShadowPass->getHistoryVisibility(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		copyLastFrameTex(mGeometryPass->getGbufferDepth(), mGeometryPass->getPrevGbufferDepth(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		copyLastFrameTex(mDDGIPass->getProbeIrradiance(),mDDGIPass->getPrevIrradiance(),VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_GENERAL);
		copyLastFrameTex(mDDGIPass->getProbeVisibility(),mDDGIPass->getPrevVisibility(), VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_LAYOUT_GENERAL);
		copyLastFrameTex(mDDGIPass->getProbeOffset(), mDDGIPass->getPrevOffset(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);
	}
}