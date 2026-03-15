#include "ddgi_pass.h"

namespace flare::renderer {

	void DDGIPass::init(const CommandPool::Ptr& commandPool, const flare::loader::MeshData& meshData, const DescriptorSetLayout::Ptr& frameSetLayout,
						const DescriptorSetLayout::Ptr& staticSetLayout, const FrameGraph& frameGraph, const GeometryPass::Ptr& geometryPass,
						const CubeMapTexture::Ptr& skybox, int frameCount) {
		mFrameCount = frameCount;
		mSkybox = skybox;
		mNormal = geometryPass->getGbufferNormal();
		mDepth = geometryPass->getGbufferDepth();
		
		createImages(commandPool, frameGraph);
		createProbeRTDS(meshData);
		createOffsetDS();
		createIrradianceDS();
		createVisibilityDS();
		createSampleIrradianceDS();
		createPipelines(frameSetLayout, staticSetLayout);
	}

	void DDGIPass::createImages(const CommandPool::Ptr& commandPool, const FrameGraph& frameGraph) {
		// radiance
		SamplerDesc linearClamp{};
		linearClamp.minFilter = VK_FILTER_LINEAR;
		linearClamp.magFilter = VK_FILTER_LINEAR;
		linearClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		linearClamp.anisotropyEnable = VK_FALSE;
		linearClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		linearClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		linearClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

		mDDGI.probeCounts.w = mDDGI.probeCounts.x * mDDGI.probeCounts.y * mDDGI.probeCounts.z;
		int radianceW = mDDGI.probeRays;
		int radianceH = (int)mDDGI.probeCounts.w;
		auto radianceImage = Image::create(mDevice, radianceW, radianceH,
			                 VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage,
						     VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		mRayTraceRadiance = Texture::create(mDevice, radianceImage, linearClamp);
		
		// offset
		VkImageUsageFlags offsetUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		for (int i = 0; i < 2; ++i) {
			auto offsetImage = Image::create(mDevice, mDDGI.probeCounts.x * mDDGI.probeCounts.y, mDDGI.probeCounts.z, VK_FORMAT_R16G16B16A16_SFLOAT,
										     VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, offsetUsage,
										     VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			mOffset[i] = Texture::create(mDevice, offsetImage, linearClamp);
			VkImageUsageFlags prevOffsetUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			auto prevOffsetImage = Image::create(mDevice, mDDGI.probeCounts.x * mDDGI.probeCounts.y, mDDGI.probeCounts.z, VK_FORMAT_R16G16B16A16_SFLOAT,
												VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, prevOffsetUsage,
												VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			mPrevOffset = Texture::create(mDevice, prevOffsetImage, linearClamp);
		}

		// irradiance and visibility
		SamplerDesc nearestClamp = linearClamp;
		nearestClamp.minFilter = VK_FILTER_NEAREST;
		nearestClamp.magFilter = VK_FILTER_NEAREST;
		nearestClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		int octahedralIrradianceSize = mDDGI.irradianceSideLength + 2;
		mDDGI.irradianceWidth = octahedralIrradianceSize * (mDDGI.probeCounts.x * mDDGI.probeCounts.y);
		mDDGI.irradianceHeight = octahedralIrradianceSize * (mDDGI.probeCounts.z);
		
		for (int i = 0; i < 2; ++i) {
			VkImageUsageFlags currUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			auto irrImage = Image::create(mDevice, mDDGI.irradianceWidth, mDDGI.irradianceHeight,
										  VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, currUsage,
										  VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			mProbeIrradiance[i] = Texture::create(mDevice, irrImage, linearClamp);

			VkImageUsageFlags prevUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			auto prevIrrImage = Image::create(mDevice, mDDGI.irradianceWidth, mDDGI.irradianceHeight,
											 VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, prevUsage,
											 VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			mPrevIrradiance = Texture::create(mDevice, prevIrrImage, linearClamp);

			auto visibilityImage = Image::create(mDevice, mDDGI.irradianceWidth, mDDGI.irradianceHeight,
												VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, currUsage,
												VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			mVisibility[i] = Texture::create(mDevice, visibilityImage, nearestClamp);
			auto preVisImage = Image::create(mDevice, mDDGI.irradianceWidth, mDDGI.irradianceHeight,
											 VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, prevUsage,
				                             VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			mPrevVisibility = Texture::create(mDevice, preVisImage, nearestClamp);
		}

		// sample irradiance
		mDDGI.reciprocalProbeSpacing.x = 1.0f / mDDGI.probeSpacing.x;
		mDDGI.reciprocalProbeSpacing.y = 1.0f / mDDGI.probeSpacing.y;
		mDDGI.reciprocalProbeSpacing.z = 1.0f / mDDGI.probeSpacing.z;

		SamplerDesc pointClamp{};
		pointClamp.minFilter = VK_FILTER_NEAREST;
		pointClamp.magFilter = VK_FILTER_NEAREST;
		pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		pointClamp.anisotropyEnable = VK_FALSE;
		pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		mIndirectLighting = Texture::create(mDevice, frameGraph.getImage("indirect_lighting"), pointClamp);

		// initial the layout
		auto cmd = CommandBuffer::create(mDevice, commandPool);
		cmd->begin();
		cmd->transitionImageLayout(mRayTraceRadiance->getImage()->getImage(), mRayTraceRadiance->getImage()->getFormat(),
								  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
								  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
		for (int i = 0; i < 2; ++i) {
			cmd->transitionImageLayout(mOffset[i]->getImage()->getImage(), mOffset[i]->getImage()->getFormat(),
										VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
										VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			cmd->transitionImageLayout(mProbeIrradiance[i]->getImage()->getImage(), mProbeIrradiance[i]->getImage()->getFormat(),
				                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 
									   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			cmd->transitionImageLayout(mVisibility[i]->getImage()->getImage(), mVisibility[i]->getImage()->getFormat(),
										VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 
										VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
		cmd->transitionImageLayout(mPrevIrradiance->getImage()->getImage(), mPrevIrradiance->getImage()->getFormat(),
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		cmd->transitionImageLayout(mPrevVisibility->getImage()->getImage(), mPrevVisibility->getImage()->getFormat(),
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		cmd->transitionImageLayout(mPrevOffset->getImage()->getImage(), mPrevOffset->getImage()->getFormat(),
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		cmd->end();
		cmd->submitSync(mDevice->getGraphicQueue());
	}

	void DDGIPass::createProbeRTDS(const flare::loader::MeshData& meshData) {
		enum ProbeRtBinding : uint32_t {
			BindingDDGIUbo = 0,
			BindingRadiance = 1,
			BindingPosition = 2,
			BindingUv = 3,
			BindingNormal = 4,
			BindingIndex = 5,
			BindinSkybox = 6,
			BindingSubmesh = 7,
			BindingPreIrradiance = 8,
			BindingPreVisibility = 9,
			BindingProbeStatus = 10,
			BindingPreOffset = 11
		};

		std::vector<UniformParameter::Ptr> params;
		const VkShaderStageFlags kProbeRtStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
												  VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT | 
												  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		mDDGIParam = UniformParameter::create();
		mDDGIParam->mBinding = BindingDDGIUbo;
		mDDGIParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		mDDGIParam->mStage = kProbeRtStages;
		mDDGIParam->mCount = 1;
		mDDGIParam->mSize = sizeof(DDGIParams);
		params.push_back(mDDGIParam);

		for (int i = 0; i < mFrameCount; ++i) {
			auto ubo = Buffer::createUniformBuffer(mDevice, mDDGIParam->mSize, nullptr);
			mDDGIParam->mBuffers.push_back(ubo);
		}

		mRadianceParam = UniformParameter::create();
		mRadianceParam->mBinding = BindingRadiance;
		mRadianceParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		mRadianceParam->mStage = kProbeRtStages;
		mRadianceParam->mCount = 1;
		mRadianceParam->mImageInfos.resize(1);
		mRadianceParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mRadianceParam->mImageInfos[0].imageView = mRayTraceRadiance->getImageView();
		mRadianceParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
		params.push_back(mRadianceParam);

		// binding vertex position, uv, normal, index
		constexpr uint32_t kStride = 11;
		constexpr uint32_t kPosOffset = 0;
		constexpr uint32_t kUvOffset = 3;
		constexpr uint32_t kNrmOffset = 5;

		constexpr uint32_t kStrideBytes = kStride * sizeof(float);
		const size_t vertexBytes = meshData.vertexData.size();
		const size_t vertexCount = vertexBytes / kStrideBytes;
		const VkDeviceSize positionBytes = vertexCount * sizeof(glm::vec4);
		const VkDeviceSize uvBytes = vertexCount * sizeof(glm::vec2);
		const VkDeviceSize normalBytes = vertexCount * sizeof(glm::vec4);
		const VkDeviceSize indexBytes = meshData.indexData.size() * sizeof(uint32_t);

		std::vector<glm::vec4> positions(vertexCount);
		std::vector<glm::vec2> uvs(vertexCount);
		std::vector<glm::vec4> normals(vertexCount);

		const uint8_t* raw = meshData.vertexData.data();
		for (size_t i = 0; i < vertexCount; ++i) {
			const float* v = reinterpret_cast<const float*>(raw + i * kStrideBytes);
			positions[i] = glm::vec4(v[kPosOffset + 0], v[kPosOffset + 1], v[kPosOffset + 2], 1.0f);
			uvs[i] = glm::vec2(v[kUvOffset + 0], v[kUvOffset + 1]);
			normals[i] = glm::vec4(v[kNrmOffset + 0], v[kNrmOffset + 1], v[kNrmOffset + 2], 0.0f);
		}

		std::vector<SubmeshGpu> submeshes;
		submeshes.reserve(meshData.meshes.size());

		for (const auto& m : meshData.meshes) {
			SubmeshGpu s{};
			s.indexOffset = m.indexOffset;
			s.vertexOffset = m.vertexOffset;
			s.materialId = m.materialID;
			s.pad0 = 0;
			submeshes.push_back(s);
		}
		const VkDeviceSize submeshBytes = submeshes.size() * sizeof(SubmeshGpu);

		auto addStorageBufferParamPerFrame = [&](uint32_t binding, VkDeviceSize byteSize, const void* data) {
				auto param = UniformParameter::create();
				param->mBinding = binding;
				param->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				param->mStage = kProbeRtStages;
				param->mCount = 1;
				param->mSize = byteSize;

				for (int i = 0; i < mFrameCount; ++i) {
					auto buf = Buffer::createStorageBuffer(mDevice, byteSize, data, false);
					param->mBuffers.push_back(buf);
				}
				params.push_back(param);
		};

		addStorageBufferParamPerFrame(BindingPosition, positionBytes, positions.data());
		addStorageBufferParamPerFrame(BindingUv, uvBytes, uvs.data());
		addStorageBufferParamPerFrame(BindingNormal, normalBytes, normals.data());
		addStorageBufferParamPerFrame(BindingIndex, indexBytes, meshData.indexData.data());
		addStorageBufferParamPerFrame(BindingSubmesh, submeshBytes, submeshes.data());

		auto paramSkybox = UniformParameter::create();
		paramSkybox->mBinding = BindinSkybox;
		paramSkybox->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		paramSkybox->mStage = kProbeRtStages;
		paramSkybox->mCount = 1;
		paramSkybox->mImageInfos.resize(1);
		paramSkybox->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		paramSkybox->mImageInfos[0].imageView = mSkybox->getImageView();
		paramSkybox->mImageInfos[0].sampler = mSkybox->getSampler();
		params.push_back(paramSkybox);

		auto paramPrevIrr = UniformParameter::create();
		paramPrevIrr->mBinding = BindingPreIrradiance;
		paramPrevIrr->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		paramPrevIrr->mStage = kProbeRtStages;
		paramPrevIrr->mCount = 1;
		paramPrevIrr->mImageInfos.resize(1);
		paramPrevIrr->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		paramPrevIrr->mImageInfos[0].imageView = mPrevIrradiance->getImageView();
		paramPrevIrr->mImageInfos[0].sampler = mPrevIrradiance->getSampler();
		params.push_back(paramPrevIrr);

		auto paramPrevVis = UniformParameter::create();
		paramPrevVis->mBinding = BindingPreVisibility;
		paramPrevVis->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		paramPrevVis->mStage = kProbeRtStages;
		paramPrevVis->mCount = 1;
		paramPrevVis->mImageInfos.resize(1);
		paramPrevVis->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		paramPrevVis->mImageInfos[0].imageView = mPrevVisibility->getImageView();
		paramPrevVis->mImageInfos[0].sampler = mPrevVisibility->getSampler();
		params.push_back(paramPrevVis);

		mProbeStatusParam = UniformParameter::create();
		mProbeStatusParam->mBinding = BindingProbeStatus;
		mProbeStatusParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		mProbeStatusParam->mStage = kProbeRtStages;
		mProbeStatusParam->mCount = 1;
		mProbeStatusParam->mSize = sizeof(uint32_t) * static_cast<size_t>(mDDGI.probeCounts.w);
		params.push_back(mProbeStatusParam);

		std::vector<uint32_t> initStatus(mDDGI.probeCounts.w, PROBE_STATUS_UNINITIALIZED);
		auto statusBuffer =Buffer::createStorageBuffer(mDevice, mProbeStatusParam->mSize, initStatus.data(), false);
		for (int i = 0; i < mFrameCount; ++i) {
			mProbeStatusParam->mBuffers.push_back(statusBuffer);
		}

		auto paramPrevOffset = UniformParameter::create();
		paramPrevOffset->mBinding = BindingPreOffset;
		paramPrevOffset->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		paramPrevOffset->mStage = kProbeRtStages;
		paramPrevOffset->mCount = 1;
		paramPrevOffset->mImageInfos.resize(1);
		paramPrevOffset->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		paramPrevOffset->mImageInfos[0].imageView = mPrevOffset->getImageView();
		paramPrevOffset->mImageInfos[0].sampler = mPrevOffset->getSampler();
		params.push_back(paramPrevOffset);

		mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout->build(params);
		mDescriptorPool = DescriptorPool::create(mDevice);
		mDescriptorPool->build(params, mFrameCount);
		mDescriptorSet = DescriptorSet::create(mDevice, params, mDescriptorSetLayout, mDescriptorPool, mFrameCount);

		for (int i = 0; i < mFrameCount; ++i) {
			VkDescriptorSet set = mDescriptorSet->getDescriptorSet(i);
			mDescriptorSet->updateStorageImage(set, mRadianceParam->mBinding, mRadianceParam->mImageInfos[0]);
		}
	}

	void DDGIPass::createOffsetDS() {
		enum OffsetBinding : uint32_t {
			BindingRadianceOffset = 0,
			BindingOffsetOut = 1,
			BindingOffsetIn = 2,
		};

		const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT;
		std::vector<UniformParameter::Ptr> params;

		mRadianceForOffsetParam = UniformParameter::create();
		mRadianceForOffsetParam->mBinding = BindingRadianceOffset;
		mRadianceForOffsetParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mRadianceForOffsetParam->mStage = kStages;
		mRadianceForOffsetParam->mCount = 1;
		mRadianceForOffsetParam->mImageInfos.resize(1);
		mRadianceForOffsetParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mRadianceForOffsetParam->mImageInfos[0].imageView = mRayTraceRadiance->getImageView();
		mRadianceForOffsetParam->mImageInfos[0].sampler = mRayTraceRadiance->getSampler();
		params.push_back(mRadianceForOffsetParam);

		auto addStorageImage = [&](uint32_t binding, Texture::Ptr texture, UniformParameter::Ptr& outParam) {
			outParam = UniformParameter::create();
			outParam->mBinding = binding;
			outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			outParam->mStage = kStages;
			outParam->mCount = 1;
			outParam->mImageInfos.resize(1);
			outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outParam->mImageInfos[0].imageView = texture->getImageView();
			outParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
			params.push_back(outParam);
		};

		addStorageImage(BindingOffsetOut, mOffset[0], mOffsetWriteParam);
		addStorageImage(BindingOffsetIn, mOffset[1], mOffsetReadParam);
		mOffsetSetLayout = DescriptorSetLayout::create(mDevice);
		mOffsetSetLayout->build(params);
		mOffsetPool = DescriptorPool::create(mDevice);
		mOffsetPool->build(params, mFrameCount);
		mOffsetSet = DescriptorSet::create(mDevice, params, mOffsetSetLayout, mOffsetPool, mFrameCount);

		for (uint32_t i = 0; i < mFrameCount; ++i) {
			VkDescriptorSet set = mOffsetSet->getDescriptorSet(i);
			mOffsetSet->updateImage(set, BindingRadianceOffset, mRadianceForOffsetParam->mImageInfos[0]);
			mOffsetSet->updateStorageImage(set, BindingOffsetOut, mOffsetWriteParam->mImageInfos[0]);
			mOffsetSet->updateStorageImage(set, BindingOffsetIn, mOffsetReadParam->mImageInfos[0]);
		}
	}

	void DDGIPass::createIrradianceDS() {
		enum IrradianceBinding : uint32_t {
			BindingRadiance = 0,
			BindingIrradianceOut = 1,
			BindingIrradianceIn = 2,
		};

		const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT;
		std::vector<UniformParameter::Ptr> params;
		
		mRadianceForIrrParam = UniformParameter::create();
		mRadianceForIrrParam->mBinding = BindingRadiance;
		mRadianceForIrrParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mRadianceForIrrParam->mStage = kStages;
		mRadianceForIrrParam->mCount = 1;
		mRadianceForIrrParam->mImageInfos.resize(1);
		mRadianceForIrrParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mRadianceForIrrParam->mImageInfos[0].imageView = mRayTraceRadiance->getImageView();
		mRadianceForIrrParam->mImageInfos[0].sampler = mRayTraceRadiance->getSampler();
		params.push_back(mRadianceForIrrParam);

		auto addStorageImage = [&](uint32_t binding, Texture::Ptr texture, UniformParameter::Ptr& outParam) {
				outParam = UniformParameter::create();
				outParam->mBinding = binding;
				outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				outParam->mStage = kStages;
				outParam->mCount = 1;
				outParam->mImageInfos.resize(1);
				outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				outParam->mImageInfos[0].imageView = texture->getImageView();
				outParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
				params.push_back(outParam);
		};

		addStorageImage(BindingIrradianceOut,mProbeIrradiance[0], mIrradianceWriteParam);
		addStorageImage(BindingIrradianceIn, mProbeIrradiance[1], mIrradianceReadParam);

		mIrradianceSetLayout = DescriptorSetLayout::create(mDevice);
		mIrradianceSetLayout->build(params);
		mIrradiancePool = DescriptorPool::create(mDevice);
		mIrradiancePool->build(params, mFrameCount);
		mIrradianceSet = DescriptorSet::create(mDevice,params, mIrradianceSetLayout, mIrradiancePool, mFrameCount);

		for (uint32_t i = 0; i < mFrameCount; ++i) {
			VkDescriptorSet set = mIrradianceSet->getDescriptorSet(i);
			mIrradianceSet->updateImage(set, BindingRadiance, mRadianceForIrrParam->mImageInfos[0]);
			mIrradianceSet->updateStorageImage(set, BindingIrradianceOut, mIrradianceWriteParam->mImageInfos[0]);
			mIrradianceSet->updateStorageImage(set, BindingIrradianceIn, mIrradianceReadParam->mImageInfos[0]);
		}
	}

	void DDGIPass::createVisibilityDS() {
		enum VisibilityBinding : uint32_t {
			BindingRadianceVis = 0,
			BindingVisibilityOut = 1,
			BindingVisibilityIn = 2,
		};

		const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT;
		std::vector<UniformParameter::Ptr> params;

		mRadianceForVisParam = UniformParameter::create();
		mRadianceForVisParam->mBinding = BindingRadianceVis;
		mRadianceForVisParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mRadianceForVisParam->mStage = kStages;
		mRadianceForVisParam->mCount = 1;
		mRadianceForVisParam->mImageInfos.resize(1);
		mRadianceForVisParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mRadianceForVisParam->mImageInfos[0].imageView = mRayTraceRadiance->getImageView();
		mRadianceForVisParam->mImageInfos[0].sampler = mRayTraceRadiance->getSampler();
		params.push_back(mRadianceForVisParam);

		auto addStorageImage = [&](uint32_t binding, Texture::Ptr texture, UniformParameter::Ptr& outParam) {
			outParam = UniformParameter::create();
			outParam->mBinding = binding;
			outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			outParam->mStage = kStages;
			outParam->mCount = 1;
			outParam->mImageInfos.resize(1);
			outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outParam->mImageInfos[0].imageView = texture->getImageView();
			outParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
			params.push_back(outParam);
			};

		addStorageImage(BindingVisibilityOut, mVisibility[0], mVisibilityWriteParam);
		addStorageImage(BindingVisibilityIn, mVisibility[1], mVisibilityReadParam);

		mVisibilitySetLayout = DescriptorSetLayout::create(mDevice);
		mVisibilitySetLayout->build(params);
		mVisibilityPool = DescriptorPool::create(mDevice);
		mVisibilityPool->build(params, mFrameCount);
		mVisibilitySet = DescriptorSet::create(mDevice, params, mVisibilitySetLayout, mVisibilityPool, mFrameCount);

		for (uint32_t i = 0; i < mFrameCount; ++i) {
			VkDescriptorSet set = mVisibilitySet->getDescriptorSet(i);
			mVisibilitySet->updateImage(set, BindingRadianceVis, mRadianceForVisParam->mImageInfos[0]);
			mVisibilitySet->updateStorageImage(set, BindingVisibilityOut, mVisibilityWriteParam->mImageInfos[0]);
			mVisibilitySet->updateStorageImage(set, BindingVisibilityIn, mVisibilityReadParam->mImageInfos[0]);
		}
	}

	void DDGIPass::createSampleIrradianceDS() {
		enum SampleIrradianceBinding : uint32_t {
			BindingNormal = 0,
			BindingDepth = 1,
			BindingIrradiance = 2,
			BindingVisibility = 3,
			BindingIndirectOut = 4,
			BindingOffset = 5,
		};
		const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT;
		std::vector<UniformParameter::Ptr> params;

		mNormalParam = UniformParameter::create();
		mNormalParam->mBinding = BindingNormal;
		mNormalParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mNormalParam->mStage = kStages;
		mNormalParam->mCount = 1;
		mNormalParam->mImageInfos.resize(1);
		mNormalParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mNormalParam->mImageInfos[0].imageView = mNormal->getImageView();
		mNormalParam->mImageInfos[0].sampler = mNormal->getSampler();
		params.push_back(mNormalParam);

		mDepthParam = UniformParameter::create();
		mDepthParam->mBinding = BindingDepth;
		mDepthParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mDepthParam->mStage = kStages;
		mDepthParam->mCount = 1;
		mDepthParam->mImageInfos.resize(1);
		mDepthParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mDepthParam->mImageInfos[0].imageView = mDepth->getImageView();
		mDepthParam->mImageInfos[0].sampler = mDepth->getSampler();
		params.push_back(mDepthParam);

		mSampleIrradianceParam = UniformParameter::create();
		mSampleIrradianceParam->mBinding = BindingIrradiance;
		mSampleIrradianceParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mSampleIrradianceParam->mStage = kStages;
		mSampleIrradianceParam->mCount = 1;
		mSampleIrradianceParam->mImageInfos.resize(1);
		mSampleIrradianceParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mSampleIrradianceParam->mImageInfos[0].imageView = getProbeIrradiance()->getImageView();
		mSampleIrradianceParam->mImageInfos[0].sampler = getProbeIrradiance()->getSampler();
		params.push_back(mSampleIrradianceParam);

		mSampleVisibilityParam = UniformParameter::create();
		mSampleVisibilityParam->mBinding = BindingVisibility;
		mSampleVisibilityParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mSampleVisibilityParam->mStage = kStages;
		mSampleVisibilityParam->mCount = 1;
		mSampleVisibilityParam->mImageInfos.resize(1);
		mSampleVisibilityParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mSampleVisibilityParam->mImageInfos[0].imageView = getProbeVisibility()->getImageView();
		mSampleVisibilityParam->mImageInfos[0].sampler = getProbeVisibility()->getSampler();
		params.push_back(mSampleVisibilityParam);

		mIndirectParam = UniformParameter::create();
		mIndirectParam->mBinding = BindingIndirectOut;
		mIndirectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		mIndirectParam->mStage = kStages;
		mIndirectParam->mCount = 1;
		mIndirectParam->mImageInfos.resize(1);
		mIndirectParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mIndirectParam->mImageInfos[0].imageView = mIndirectLighting->getImageView();
		mIndirectParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
		params.push_back(mIndirectParam);

		auto offsetParam = UniformParameter::create();
		offsetParam->mBinding = BindingOffset;
		offsetParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		offsetParam->mStage = kStages;
		offsetParam->mCount = 1;
		offsetParam->mImageInfos.resize(1);
		offsetParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		offsetParam->mImageInfos[0].imageView = mPrevOffset->getImageView();
		offsetParam->mImageInfos[0].sampler = mPrevOffset->getSampler();
		params.push_back(offsetParam);

		mSampleSetLayout = DescriptorSetLayout::create(mDevice);
		mSampleSetLayout->build(params);
		mSamplePool = DescriptorPool::create(mDevice);
		mSamplePool->build(params, mFrameCount);
		mSampleSet = DescriptorSet::create(mDevice, params, mSampleSetLayout, mSamplePool, mFrameCount);

		for (uint32_t i = 0; i < mFrameCount; ++i) {
			VkDescriptorSet set = mSampleSet->getDescriptorSet(i);
			mSampleSet->updateImage(set, BindingNormal, mNormalParam->mImageInfos[0]);
			mSampleSet->updateImage(set, BindingDepth, mDepthParam->mImageInfos[0]);
			mSampleSet->updateImage(set, BindingIrradiance, mSampleIrradianceParam->mImageInfos[0]);
			mSampleSet->updateImage(set, BindingVisibility, mSampleVisibilityParam->mImageInfos[0]);
			mSampleSet->updateStorageImage(set, BindingIndirectOut, mIndirectParam->mImageInfos[0]);
		}
	}

	void DDGIPass::createPipelines(const DescriptorSetLayout::Ptr& frameSetLayout, const DescriptorSetLayout::Ptr& staticSetLayout) {
		auto ddgiRayGen = Shader::create(mDevice, "shaders/ddgi/probe_rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main");
		auto ddgiMiss = Shader::create(mDevice, "shaders/ddgi/probe_rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR, "main");
		auto ddgiCloseHit = Shader::create(mDevice, "shaders/ddgi/probe_rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main");
		mProbeRTPipeline = RayTracingPipeline::create(mDevice);
		mProbeRTPipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), staticSetLayout->getLayout(), mDescriptorSetLayout->getLayout()});
		mProbeRTPipeline->setMaxRecursionDepth(1);
		mProbeRTPipeline->setShaders(ddgiRayGen, std::vector<Shader::Ptr>{ ddgiMiss }, std::vector<Shader::Ptr>{ ddgiCloseHit });
		VkPushConstantRange range{};
		range.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; 
		range.offset = 0;
		range.size = sizeof(glm::vec4);
		mProbeRTPipeline->setPushConstantRanges({ range });
		mProbeRTPipeline->build();

		mOffsetPipeline = ComputePipeline::create(mDevice);
		auto offsetShader = Shader::create(mDevice, "shaders/ddgi/probe_offset_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mOffsetPipeline->setShader(offsetShader);
		mOffsetPipeline->setDescriptorSetLayouts({ mDescriptorSetLayout->getLayout(), mOffsetSetLayout->getLayout() });
		mOffsetPipeline->setPushConstantRange(0, sizeof(uint32_t), VK_SHADER_STAGE_COMPUTE_BIT);
		mOffsetPipeline->build();

		mIrradiancePipeline = ComputePipeline::create(mDevice);
		auto irradiancelShader = Shader::create(mDevice, "shaders/ddgi/update_irradiance_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mIrradiancePipeline->setShader(irradiancelShader);
		mIrradiancePipeline->setDescriptorSetLayouts({ mDescriptorSetLayout->getLayout(), mIrradianceSetLayout->getLayout() });
		mIrradiancePipeline->build();

		mVisibilityPipeline = ComputePipeline::create(mDevice);
		auto visibilitylShader = Shader::create(mDevice, "shaders/ddgi/update_visibility_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mVisibilityPipeline->setShader(visibilitylShader);
		mVisibilityPipeline->setDescriptorSetLayouts({ mDescriptorSetLayout->getLayout(), mVisibilitySetLayout->getLayout() });
		mVisibilityPipeline->build();

		mProbeStatusPipeline = ComputePipeline::create(mDevice);
		auto probeStatusShader = Shader::create(mDevice, "shaders/ddgi/probe_status_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mProbeStatusPipeline->setShader(probeStatusShader);
		mProbeStatusPipeline->setDescriptorSetLayouts({ mDescriptorSetLayout->getLayout() });
		mProbeStatusPipeline->setPushConstantRange(0, sizeof(uint32_t), VK_SHADER_STAGE_COMPUTE_BIT);
		mProbeStatusPipeline->build();

		mSamplePipeline = ComputePipeline::create(mDevice);
		auto sampleShader = Shader::create(mDevice, "shaders/ddgi/sample_irradiance_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mSamplePipeline->setShader(sampleShader);
		mSamplePipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), mDescriptorSetLayout->getLayout(), mSampleSetLayout->getLayout() });
		mSamplePipeline->setPushConstantRange(0, sizeof(glm::vec4), VK_SHADER_STAGE_COMPUTE_BIT);
		mSamplePipeline->build();
	}

	void DDGIPass::render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex, Camera camera) {
		probeRayTrace(cmd, frameSet, staticSet, frameIndex, camera);
		updateProbeOffset(cmd, frameIndex);
		updateIrradiance(cmd, frameIndex);
		updateVisibility(cmd, frameIndex);
		sampleIrradiance(cmd, frameSet, frameIndex, camera);
	}

	void DDGIPass::probeRayTrace(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, const DescriptorSet::Ptr& staticSet, int frameIndex, Camera camera) {
		
		const float rotationScaler = 0.001f;
		glm::vec3 eulerAngles{
		  getRandomValue(-1.0f, 1.0f) * rotationScaler,
		  getRandomValue(-1.0f, 1.0f) * rotationScaler, 
		  getRandomValue(-1.0f, 1.0f) * rotationScaler
		};

		glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), eulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), eulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), eulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
		mDDGI.randomRotation = rotationX * rotationY * rotationZ;

		updateBuffer(mDescriptorSet, frameIndex);
		
		cmd->bindRayTracingPipeline(mProbeRTPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mProbeRTPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mProbeRTPipeline->getLayout(), staticSet->getDescriptorSet(0), 1);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mProbeRTPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 2);
		
		glm::vec4 cameraPos = glm::vec4(camera.getPosition(), 1.0f);
		cmd->pushConstants(mProbeRTPipeline->getLayout(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, cameraPos);

		mDevice->getRT().vkCmdTraceRaysKHR(cmd->getCommandBuffer(), &mProbeRTPipeline->getRgenRegion(), &mProbeRTPipeline->getMissRegion(), 
										   &mProbeRTPipeline->getHitRegion(), &mProbeRTPipeline->getCallableRegion(), mDDGI.probeRays, mDDGI.probeCounts.w, 1);

		cmd->transitionImageLayout(mRayTraceRadiance->getImage()->getImage(), mRayTraceRadiance->getImage()->getFormat(),
								   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void DDGIPass::updateProbeOffset(const CommandBuffer::Ptr& cmd, int frameIndex) {
		const uint32_t previousIndex = mOffsetPingPongIndex;
		mOffsetPingPongIndex = (mOffsetPingPongIndex + 1) % 2;

		Texture::Ptr offsetReadTex = (previousIndex == 0) ? mOffset[0] : mOffset[1];
		Texture::Ptr offsetWriteTex = (mOffsetPingPongIndex == 0) ? mOffset[0] : mOffset[1];

		VkDescriptorImageInfo writeInfo{};
		writeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writeInfo.imageView = offsetWriteTex->getImageView();
		writeInfo.sampler = VK_NULL_HANDLE;

		VkDescriptorImageInfo readInfo{};
		readInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		readInfo.imageView = offsetReadTex->getImageView();
		readInfo.sampler = VK_NULL_HANDLE;

		auto ds = mOffsetSet->getDescriptorSet(frameIndex);
		mOffsetSet->updateStorageImage(ds, mOffsetWriteParam->mBinding, writeInfo);
		mOffsetSet->updateStorageImage(ds, mOffsetReadParam->mBinding, readInfo);

		cmd->bindComputePipeline(mOffsetPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mOffsetPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mOffsetPipeline->getLayout(), mOffsetSet->getDescriptorSet(frameIndex), 1);

		uint32_t pc = mFirstOffsetFrame ? 1u : 0u;
		cmd->pushConstants(mOffsetPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, pc);
		const uint32_t totalProbes = uint32_t(mDDGI.probeCounts.w);
		const uint32_t groupX = (totalProbes + 31) / 32;
		cmd->dispatch(groupX, 1, 1);
		mFirstOffsetFrame = false;
		cmd->transitionImageLayout(offsetWriteTex->getImage()->getImage(), offsetWriteTex->getImage()->getFormat(),
								   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
								   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void DDGIPass::updateProbeStatus(const CommandBuffer::Ptr& cmd, int frameIndex) {
		cmd->bindComputePipeline(mProbeStatusPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mProbeStatusPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 0);
		uint32_t firstFrame = mProbeStatusFirstFrame ? 1u : 0u;
		cmd->pushConstants(mProbeStatusPipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, firstFrame);
		const uint32_t totalProbes = static_cast<uint32_t>(mDDGI.probeCounts.w);
		const uint32_t groupX = (totalProbes + 31) / 32;
		cmd->dispatch(groupX, 1, 1);
		mProbeStatusFirstFrame = false;

		cmd->transitionImageLayout(mRayTraceRadiance->getImage()->getImage(), mRayTraceRadiance->getImage()->getFormat(),
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void DDGIPass::updateIrradiance(const CommandBuffer::Ptr& cmd, int frameIndex) {
		const uint32_t previousIndex = mIrradiancePingPongIndex;
		mIrradiancePingPongIndex = (mIrradiancePingPongIndex + 1) % 2;

		Texture::Ptr irradianceReadTex = (previousIndex == 0) ? mProbeIrradiance[0] : mProbeIrradiance[1];
		Texture::Ptr irradianceWriteTex = (mIrradiancePingPongIndex == 0) ? mProbeIrradiance[0] : mProbeIrradiance[1];

		VkDescriptorImageInfo writeInfo{};
		writeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writeInfo.imageView = irradianceWriteTex->getImageView();
		writeInfo.sampler = VK_NULL_HANDLE;

		VkDescriptorImageInfo readInfo{};
		readInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		readInfo.imageView = irradianceReadTex->getImageView();
		readInfo.sampler = VK_NULL_HANDLE;

		auto ds = mIrradianceSet->getDescriptorSet(frameIndex);
		mIrradianceSet->updateStorageImage(ds, mIrradianceWriteParam->mBinding, writeInfo);
		mIrradianceSet->updateStorageImage(ds, mIrradianceReadParam->mBinding, readInfo);

		cmd->bindComputePipeline(mIrradiancePipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mIrradiancePipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mIrradiancePipeline->getLayout(), mIrradianceSet->getDescriptorSet(frameIndex), 1);

		const uint32_t atlasW = uint32_t(mDDGI.irradianceWidth);
		const uint32_t atlasH = uint32_t(mDDGI.irradianceHeight);

		const uint32_t groupX = (atlasW + 7) / 8;
		const uint32_t groupY = (atlasH + 7) / 8;
		cmd->dispatch(groupX, groupY, 1);

		cmd->transitionImageLayout(irradianceWriteTex->getImage()->getImage(), irradianceWriteTex->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
								   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void DDGIPass::updateVisibility(const CommandBuffer::Ptr& cmd, int frameIndex) {
		const uint32_t previousIndex = mVisibilityPingPongIndex;
		mVisibilityPingPongIndex = (mVisibilityPingPongIndex + 1) % 2;

		Texture::Ptr visibilityReadTex = (previousIndex == 0) ? mVisibility[0] : mVisibility[1];
		Texture::Ptr visibilityWriteTex = (mVisibilityPingPongIndex == 0) ? mVisibility[0] : mVisibility[1];

		VkDescriptorImageInfo writeInfo{};
		writeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writeInfo.imageView = visibilityWriteTex->getImageView();
		writeInfo.sampler = VK_NULL_HANDLE;

		VkDescriptorImageInfo readInfo{};
		readInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		readInfo.imageView = visibilityReadTex->getImageView();
		readInfo.sampler = VK_NULL_HANDLE;

		auto ds = mVisibilitySet->getDescriptorSet(frameIndex);
		mVisibilitySet->updateStorageImage(ds, mVisibilityWriteParam->mBinding, writeInfo);
		mVisibilitySet->updateStorageImage(ds, mVisibilityReadParam->mBinding, readInfo);

		cmd->bindComputePipeline(mVisibilityPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mVisibilityPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mVisibilityPipeline->getLayout(), mVisibilitySet->getDescriptorSet(frameIndex), 1);

		const uint32_t atlasW = uint32_t(mDDGI.irradianceWidth);
		const uint32_t atlasH = uint32_t(mDDGI.irradianceHeight);

		const uint32_t groupX = (atlasW + 7) / 8;
		const uint32_t groupY = (atlasH + 7) / 8;
		cmd->dispatch(groupX, groupY, 1);

		cmd->transitionImageLayout(mRayTraceRadiance->getImage()->getImage(), mRayTraceRadiance->getImage()->getFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

		cmd->transitionImageLayout(visibilityWriteTex->getImage()->getImage(), visibilityWriteTex->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void DDGIPass::sampleIrradiance(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, int frameIndex, Camera camera) {
		
		VkDescriptorImageInfo irradianceInfo{};
		irradianceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		irradianceInfo.imageView = getProbeIrradiance()->getImageView();
		irradianceInfo.sampler = getProbeIrradiance()->getSampler();

		VkDescriptorImageInfo visibilityInfo{};
		visibilityInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		visibilityInfo.imageView = getProbeVisibility()->getImageView();
		visibilityInfo.sampler = getProbeVisibility()->getSampler();

		VkDescriptorSet set = mSampleSet->getDescriptorSet(frameIndex);
		mSampleSet->updateImage(set, mSampleIrradianceParam->mBinding, irradianceInfo);
		mSampleSet->updateImage(set, mSampleVisibilityParam->mBinding, visibilityInfo);

		cmd->transitionImageLayout(mIndirectLighting->getImage()->getImage(), mIndirectLighting->getImage()->getFormat(),
								   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		cmd->bindComputePipeline(mSamplePipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mSamplePipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mSamplePipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 1);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mSamplePipeline->getLayout(), mSampleSet->getDescriptorSet(frameIndex), 2);
		
		glm::vec4 cameraPos = glm::vec4(camera.getPosition(), 1.0f);
		cmd->pushConstants(mSamplePipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, cameraPos);

		const uint32_t groupX = (mWidth + 7) / 8;
		const uint32_t groupY = (mHeight + 7) / 8;
		cmd->dispatch(groupX, groupY, 1);

		cmd->transitionImageLayout(mIndirectLighting->getImage()->getImage(), mIndirectLighting->getImage()->getFormat(),
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
;
		cmd->transitionImageLayout(getProbeIrradiance()->getImage()->getImage(), getProbeIrradiance()->getImage()->getFormat(), 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	void DDGIPass::updateBuffer(const DescriptorSet::Ptr& descriptorSet, int frameIndex) {

		mDDGIParam->mBuffers[frameIndex]->updateBufferByMap(&mDDGI, sizeof(DDGIParams));

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = mDDGIParam->mBuffers[frameIndex]->getBuffer();
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(DDGIParams);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = descriptorSet->getDescriptorSet(frameIndex);
		write.dstBinding = mDDGIParam->mBinding;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
	}

	float DDGIPass::getRandomValue(float min, float max) {
		float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		return min + t * (max - min);
	}	
}