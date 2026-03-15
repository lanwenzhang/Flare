#include "gtao_pass.h"

namespace flare::renderer{

    static inline uint32_t hilbertIndex(uint32_t posX, uint32_t posY){
        const uint32_t XE_HILBERT_WIDTH = 64;
        uint32_t index = 0U;
        for (uint32_t curLevel = XE_HILBERT_WIDTH / 2U; curLevel > 0U; curLevel /= 2U)
        {
            uint32_t regionX = (posX & curLevel) > 0U;
            uint32_t regionY = (posY & curLevel) > 0U;
            index += curLevel * curLevel * ((3U * regionX) ^ regionY);

            if (regionY == 0U)
            {
                if (regionX == 1U)
                {
                    posX = (XE_HILBERT_WIDTH - 1U) - posX;
                    posY = (XE_HILBERT_WIDTH - 1U) - posY;
                }
                uint32_t temp = posX;
                posX = posY;
                posY = temp;
            }
        }
        return index;
    }

	static VkImageView createViewForMipmap(Device::Ptr& device, VkImage image, VkFormat format, uint32_t baseMip) {
		VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivci.format = format;
		ivci.image = image;
		ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ivci.subresourceRange.baseMipLevel = baseMip;
		ivci.subresourceRange.levelCount = 1;
		ivci.subresourceRange.baseArrayLayer = 0;
		ivci.subresourceRange.layerCount = 1;

		VkImageView view = VK_NULL_HANDLE;
		VkResult r = vkCreateImageView(device->getDevice(), &ivci, nullptr, &view);
		if (r != VK_SUCCESS) throw std::runtime_error("GTAOPass: vkCreateImageView failed for mip view");
		return view;
	}

	GTAOPass::~GTAOPass() {
		if (!mDevice) return;
		VkDevice device = mDevice->getDevice();
		for (VkImageView view : mDepthMIPViews) {
			if (view != VK_NULL_HANDLE) {
				vkDestroyImageView(device, view, nullptr);
			}
		}
		mDepthMIPViews.clear();
	}

	void GTAOPass::init(const CommandPool::Ptr& commandPool, const GeometryPass::Ptr& geometryPass, const DescriptorSetLayout::Ptr& frameSetLayout, int frameCount) {
		mFrameCount = frameCount;
		mGDepth = geometryPass->getGbufferDepth();
		mGNormal = geometryPass->getGbufferNormal();
		createImages(commandPool);
		createPrefilerDepthDS();
        createAODS();
        createDenoiseDS();
		createPipelines(frameSetLayout);
	}

	void GTAOPass::createImages(const CommandPool::Ptr& commandPool) {
		// depth prefilter
        const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		const VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		mMipCount = 5;

		auto depthMIPImage = Image::create(mDevice, mWidth, mHeight, VK_FORMAT_R16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, 
										   usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect, 
										   0, 1, VK_IMAGE_VIEW_TYPE_2D, mMipCount);
		SamplerDesc pointClamp{};
		pointClamp.minFilter = VK_FILTER_NEAREST;
		pointClamp.magFilter = VK_FILTER_NEAREST;
		pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		pointClamp.anisotropyEnable = VK_FALSE;
		pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.minLod = 0.0f;
        pointClamp.maxLod = float(mMipCount - 1);

		mDepthMIPFilter = Texture::create(mDevice, depthMIPImage, pointClamp);

        // ao
        auto aoRawImg = Image::create(mDevice, mWidth, mHeight, VK_FORMAT_R8_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, 
                                      usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);
        auto edgesImg = Image::create(mDevice, mWidth, mHeight, VK_FORMAT_R8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                      usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);
        mAORaw = Texture::create(mDevice, aoRawImg, pointClamp);
        mEdges = Texture::create(mDevice, edgesImg, pointClamp);

        const uint32_t W = 64, H = 64;
        std::vector<uint16_t> lut(W * H);
        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                uint32_t idx = hilbertIndex(x, y);
                lut[x + W * y] = (uint16_t)idx;
            }
        }
        mHilbertLUT = Texture::create(mDevice, commandPool, W, H, VK_FORMAT_R16_UINT, lut.data(), lut.size() * sizeof(uint16_t), pointClamp);

        // ao
        auto aoPingImg = Image::create(mDevice, mWidth, mHeight, VK_FORMAT_R8_UINT,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);

        auto aoPongImg = Image::create(mDevice, mWidth, mHeight, VK_FORMAT_R8_UINT,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);

        mAO[0] = Texture::create(mDevice, aoPingImg, pointClamp);
        mAO[1] = Texture::create(mDevice, aoPongImg, pointClamp);

        // init layout
		auto cmd = CommandBuffer::create(mDevice, commandPool);
		cmd->begin();
		auto pyrImg = mDepthMIPFilter->getImage()->getImage();
		auto pyrFmt = mDepthMIPFilter->getImage()->getFormat();
		for (uint32_t mip = 0; mip < mMipCount; ++mip) {
			cmd->transitionImageLayout(pyrImg, pyrFmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, mip, 1);
		}
        cmd->transitionImageLayout(mAORaw->getImage()->getImage(), mAORaw->getImage()->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, 
                                   VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        cmd->transitionImageLayout(mEdges->getImage()->getImage(), mEdges->getImage()->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        cmd->transitionImageLayout(mAO[0]->getImage()->getImage(), mAO[0]->getImage()->getFormat(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        cmd->transitionImageLayout(mAO[1]->getImage()->getImage(), mAO[1]->getImage()->getFormat(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		cmd->end();
		cmd->submitSync(mDevice->getGraphicQueue());
	}

	void GTAOPass::createPrefilerDepthDS() {
        mGDepthParam = UniformParameter::create();
        mGDepthParam->mBinding = 0;
        mGDepthParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mGDepthParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mGDepthParam->mCount = 1;

        auto makeStorageParam = [](uint32_t binding) -> UniformParameter::Ptr {
            auto p = UniformParameter::create();
            p->mBinding = binding;
            p->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            p->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
            p->mCount = 1;
            return p;
            };

        mMip0Param = makeStorageParam(1);
        mMip1Param = makeStorageParam(2);
        mMip2Param = makeStorageParam(3);
        mMip3Param = makeStorageParam(4);
        mMip4Param = makeStorageParam(5);

        mGTAOParam = UniformParameter::create();
        mGTAOParam->mBinding = 6;
        mGTAOParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mGTAOParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mGTAOParam->mCount = 1;
        mGTAOParam->mSize = sizeof(GTAOParams);

        for (int i = 0; i < mFrameCount; ++i) {
            auto ubo = Buffer::createUniformBuffer(mDevice, mGTAOParam->mSize, nullptr);
            mGTAOParam->mBuffers.push_back(ubo);
        }

        std::vector<UniformParameter::Ptr> params{
            mGDepthParam, mMip0Param, mMip1Param, mMip2Param, mMip3Param, mMip4Param,mGTAOParam
        };

        mDepthFilterSetLayout = DescriptorSetLayout::create(mDevice);
        mDepthFilterSetLayout->build(params);
        mDepthFilterDescriptorPool = DescriptorPool::create(mDevice);
        mDepthFilterDescriptorPool->build(params, mFrameCount);

        mDepthFilterDescriptorSets = DescriptorSet::create(mDevice, params, mDepthFilterSetLayout, mDepthFilterDescriptorPool, mFrameCount);

        mDepthMIPViews.clear();
        mDepthMIPViews.resize(mMipCount, VK_NULL_HANDLE);

        VkImage  mipImg = mDepthMIPFilter->getImage()->getImage();
        VkFormat mipFmt = mDepthMIPFilter->getImage()->getFormat();

        for (uint32_t mip = 0; mip < mMipCount; ++mip) {
            mDepthMIPViews[mip] = createViewForMipmap(mDevice, mipImg, mipFmt, mip);
        }

        for (int frame = 0; frame < mFrameCount; ++frame) {
            VkDescriptorSet vkSet = mDepthFilterDescriptorSets->getDescriptorSet(frame);
            VkDescriptorImageInfo srcInfo{};
            srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcInfo.imageView = mGDepth->getImageView();
            srcInfo.sampler = mGDepth->getSampler();
            mDepthFilterDescriptorSets->updateImage(vkSet, mGDepthParam->mBinding, srcInfo);

            auto bindStorageMip = [&](uint32_t binding, uint32_t mip) {
                VkDescriptorImageInfo dst{};
                dst.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                dst.imageView = mDepthMIPViews[mip];
                dst.sampler = VK_NULL_HANDLE;
                mDepthFilterDescriptorSets->updateStorageImage(vkSet, binding, dst);
                };

            bindStorageMip(1, 0);
            bindStorageMip(2, 1);
            bindStorageMip(3, 2);
            bindStorageMip(4, 3);
            bindStorageMip(5, 4);
        }
	}

    void GTAOPass::createAODS(){
        enum AOBinding : uint32_t{
            BindingDepthMip = 0,
            BindingNormal = 1,  
            BindingHilbertLUT = 2, 
            BindingAORawOut = 3,  
            BindingEdgesOut = 4,
        };

        const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT;
        std::vector<UniformParameter::Ptr> params;

        auto addSampledImage = [&](uint32_t binding, Texture::Ptr tex, UniformParameter::Ptr& outParam){
                outParam = UniformParameter::create();
                outParam->mBinding = binding;
                outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                outParam->mStage = kStages;
                outParam->mCount = 1;

                outParam->mImageInfos.resize(1);
                outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                outParam->mImageInfos[0].imageView = tex->getImageView();
                outParam->mImageInfos[0].sampler = tex->getSampler();

                params.push_back(outParam);
        };

        auto addStorageImage = [&](uint32_t binding, Texture::Ptr tex, UniformParameter::Ptr& outParam){
                outParam = UniformParameter::create();
                outParam->mBinding = binding;
                outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                outParam->mStage = kStages;
                outParam->mCount = 1;

                outParam->mImageInfos.resize(1);
                outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                outParam->mImageInfos[0].imageView = tex->getImageView();
                outParam->mImageInfos[0].sampler = VK_NULL_HANDLE;

                params.push_back(outParam);
        };

        addSampledImage(BindingDepthMip, mDepthMIPFilter, mAODepthParam);
        addSampledImage(BindingNormal, mGNormal, mAONormalParam);
        addSampledImage(BindingHilbertLUT, mHilbertLUT, mAOHilbertParam);
        addStorageImage(BindingAORawOut, mAORaw, mAOAORawParam);
        addStorageImage(BindingEdgesOut, mEdges, mEdgesParam);

       
        mAOSetLayout = DescriptorSetLayout::create(mDevice);
        mAOSetLayout->build(params);
        mAODescriptorPool = DescriptorPool::create(mDevice);
        mAODescriptorPool->build(params, mFrameCount);
        mAODescriptorSets = DescriptorSet::create(mDevice, params, mAOSetLayout, mAODescriptorPool, mFrameCount);

        for (uint32_t i = 0; i < (uint32_t)mFrameCount; ++i){
            VkDescriptorSet set = mAODescriptorSets->getDescriptorSet(i);
            mAODescriptorSets->updateImage(set, BindingDepthMip, mAODepthParam->mImageInfos[0]);
            mAODescriptorSets->updateImage(set, BindingNormal, mAONormalParam->mImageInfos[0]);
            mAODescriptorSets->updateImage(set, BindingHilbertLUT, mAOHilbertParam->mImageInfos[0]);
            mAODescriptorSets->updateStorageImage(set, BindingAORawOut, mAOAORawParam->mImageInfos[0]);
            mAODescriptorSets->updateStorageImage(set, BindingEdgesOut, mEdgesParam->mImageInfos[0]);
        }
    }

    void GTAOPass::createDenoiseDS() {
        enum DenoiseBinding : uint32_t {
            BindingAOSrc = 0,
            BindingEdges = 1,
            BindingAOOut = 2,
        };

        const VkShaderStageFlags kStages = VK_SHADER_STAGE_COMPUTE_BIT;
        std::vector<UniformParameter::Ptr> params;

        auto addSampledImage = [&](uint32_t binding, Texture::Ptr tex, UniformParameter::Ptr& outParam) {
            outParam = UniformParameter::create();
            outParam->mBinding = binding;
            outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            outParam->mStage = kStages;
            outParam->mCount = 1;

            outParam->mImageInfos.resize(1);
            outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            outParam->mImageInfos[0].imageView = tex->getImageView();
            outParam->mImageInfos[0].sampler = tex->getSampler();

            params.push_back(outParam);
            };

        auto addStorageImage = [&](uint32_t binding, Texture::Ptr tex, UniformParameter::Ptr& outParam) {
            outParam = UniformParameter::create();
            outParam->mBinding = binding;
            outParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            outParam->mStage = kStages;
            outParam->mCount = 1;

            outParam->mImageInfos.resize(1);
            outParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outParam->mImageInfos[0].imageView = tex->getImageView();
            outParam->mImageInfos[0].sampler = VK_NULL_HANDLE;

            params.push_back(outParam);
            };

        addSampledImage(BindingAOSrc, mAORaw, mDenoiseAOSrcParam);
        addSampledImage(BindingEdges, mEdges, mDenoiseEdgesParam);
        addStorageImage(BindingAOOut, mAO[mAOPingPongIndex], mDenoiseAOOutParam);

        mDenoiseSetLayout = DescriptorSetLayout::create(mDevice);
        mDenoiseSetLayout->build(params);

        const int iterations = std::max(mDenoisePasses, 1);
        const int setCount = mFrameCount * iterations;

        mDenoiseDescriptorPool = DescriptorPool::create(mDevice);
        mDenoiseDescriptorPool->build(params, setCount);

        mDenoiseDescriptorSets.resize(iterations);

        for (int it = 0; it < iterations; ++it){
            mDenoiseDescriptorSets[it] = DescriptorSet::create(mDevice, params, mDenoiseSetLayout, mDenoiseDescriptorPool, mFrameCount);
            for (int f = 0; f < mFrameCount; ++f){
                VkDescriptorSet set = mDenoiseDescriptorSets[it]->getDescriptorSet(f);
                mDenoiseDescriptorSets[it]->updateImage(set, BindingAOSrc, mDenoiseAOSrcParam->mImageInfos[0]);
                mDenoiseDescriptorSets[it]->updateImage(set, BindingEdges, mDenoiseEdgesParam->mImageInfos[0]);
                mDenoiseDescriptorSets[it]->updateStorageImage(set, BindingAOOut, mDenoiseAOOutParam->mImageInfos[0]);
            }
        }
    }

	void GTAOPass::createPipelines(const DescriptorSetLayout::Ptr& frameSetLayout) {
		mDepthFilterPipeline = ComputePipeline::create(mDevice);
		auto cs = Shader::create(mDevice, "shaders/gtao/prefilter_depth_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mDepthFilterPipeline->setShader(cs);
		mDepthFilterPipeline->setDescriptorSetLayouts({ mDepthFilterSetLayout->getLayout() });
		mDepthFilterPipeline->build();

        mAOPipeline = ComputePipeline::create(mDevice);
        auto aoShader = Shader::create(mDevice, "shaders/gtao/ao_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
        mAOPipeline->setShader(aoShader);
        mAOPipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), mDepthFilterSetLayout->getLayout(), mAOSetLayout->getLayout() });
        mAOPipeline->build();

        mDenoisePipeline = ComputePipeline::create(mDevice);
        auto deShader = Shader::create(mDevice, "shaders/gtao/denoise_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
        mDenoisePipeline->setShader(deShader);
        mDenoisePipeline->setDescriptorSetLayouts({ mDepthFilterSetLayout->getLayout(), mDenoiseSetLayout->getLayout() });
        mDenoisePipeline->setPushConstantRange(0, (sizeof(uint32_t)), VK_SHADER_STAGE_COMPUTE_BIT);
        mDenoisePipeline->build();
	}

	void GTAOPass::render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, Camera camera, int frameIndex) {
		depthMipPrefilter(cmd, camera, frameIndex);
        ambientOcclusion(cmd, frameSet, camera, frameIndex);
        denoiseAO(cmd, frameIndex);
	}

	void GTAOPass::depthMipPrefilter(const CommandBuffer::Ptr& cmd, Camera camera, int frameIndex) {
        
        glm::vec2 pixelSize = glm::vec2(1.0f / float(mWidth), 1.0f / float(mHeight));
        glm::mat4 P = camera.getProjectMatrix();
        const float P00 = P[0][0];
        const float P11 = P[1][1];
        const float P20 = P[2][0];
        const float P21 = P[2][1];

        mGTAO.resoluton = glm::vec2(float(mWidth), float(mHeight));
        mGTAO.depthUnpack = glm::vec2(P[2][3], P[2][2]);
        mGTAO.ndcToViewMul = glm::vec2(2.0f / P00, 2.0f / P11);
        mGTAO.ndcToViewAdd = glm::vec2((-1.0f - P20) / P00, (-1.0f - P21) / P11);
        mGTAO.ndcToViewMul_x_PixelSize = mGTAO.ndcToViewMul * pixelSize;
        mGTAO.worldToView = camera.getViewMatrix();

        updateBuffer(mDepthFilterDescriptorSets, frameIndex);
		cmd->bindComputePipeline(mDepthFilterPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mDepthFilterPipeline->getLayout(), mDepthFilterDescriptorSets->getDescriptorSet(frameIndex), 0);

		uint32_t dispatchX = (mWidth + 16 - 1) / 16;
		uint32_t dispatchY = (mHeight + 16 - 1) / 16;
		cmd->dispatch(dispatchX, dispatchY, 1);

        auto pyrImg = mDepthMIPFilter->getImage()->getImage();
        auto pyrFmt = mDepthMIPFilter->getImage()->getFormat();
        for (uint32_t mip = 0; mip < mMipCount; ++mip) {
            cmd->transitionImageLayout(pyrImg, pyrFmt, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, mip, 1);
        }
	}

	void GTAOPass::ambientOcclusion(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, Camera camera, int frameIndex) {
        
        cmd->bindComputePipeline(mAOPipeline->getPipeline());

        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mAOPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mAOPipeline->getLayout(), mDepthFilterDescriptorSets->getDescriptorSet(frameIndex), 1);
        cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mAOPipeline->getLayout(), mAODescriptorSets->getDescriptorSet(frameIndex), 2);

        uint32_t dispatchX = (mWidth + 8 - 1) / 8;
        uint32_t dispatchY = (mHeight + 8 - 1) / 8;
        cmd->dispatch(dispatchX, dispatchY, 1);

        auto pyrImg = mDepthMIPFilter->getImage()->getImage();
        auto pyrFmt = mDepthMIPFilter->getImage()->getFormat();
        for (uint32_t mip = 0; mip < mMipCount; ++mip) {
            cmd->transitionImageLayout(pyrImg, pyrFmt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, mip, 1);
        }

        cmd->transitionImageLayout(mAORaw->getImage()->getImage(), mAORaw->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        cmd->transitionImageLayout(mEdges->getImage()->getImage(), mEdges->getImage()->getFormat(), VK_IMAGE_LAYOUT_GENERAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

    void GTAOPass::denoiseAO(const CommandBuffer::Ptr& cmd, int frameIndex)
    {
        Texture::Ptr inputTex = nullptr;
        Texture::Ptr outputTex = nullptr;

        const int passes = std::max(1, mDenoisePasses);

        for (int i = 0; i < passes; ++i){
            if (i == 0){
                inputTex = mAORaw;
                mAOPingPongIndex = 0;
                outputTex = mAO[mAOPingPongIndex];
            }
            else
            {
                const bool ping = (i % 2) == 1;
                inputTex = ping ? mAO[0] : mAO[1];
                outputTex = ping ? mAO[1] : mAO[0];

                VkDescriptorImageInfo aoReadInfo{};
                aoReadInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                aoReadInfo.imageView = inputTex->getImageView();
                aoReadInfo.sampler = inputTex->getSampler();

                VkDescriptorImageInfo aoWriteInfo{};
                aoWriteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                aoWriteInfo.imageView = outputTex->getImageView();
                aoWriteInfo.sampler = VK_NULL_HANDLE;

                VkDescriptorSet set = mDenoiseDescriptorSets[i]->getDescriptorSet(frameIndex);
                mDenoiseDescriptorSets[i]->updateImage(set, mDenoiseAOSrcParam->mBinding, aoReadInfo);
                mDenoiseDescriptorSets[i]->updateStorageImage(set, mDenoiseAOOutParam->mBinding, aoWriteInfo);
            }

            cmd->transitionImageLayout(
                outputTex->getImage()->getImage(),
                outputTex->getImage()->getFormat(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            cmd->bindComputePipeline(mDenoisePipeline->getPipeline());
            cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mDenoisePipeline->getLayout(),
                mDepthFilterDescriptorSets->getDescriptorSet(frameIndex), 0);
            cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mDenoisePipeline->getLayout(),
                mDenoiseDescriptorSets[i]->getDescriptorSet(frameIndex), 1);

            const uint32_t finalApply = (i == passes - 1) ? 1u : 0u;
            cmd->pushConstants(mDenoisePipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, finalApply);

            const uint32_t groupX = ((mWidth + 1) / 2 + 7) / 8;
            const uint32_t groupY = (mHeight + 7) / 8;
            cmd->dispatch(groupX, groupY, 1);

            cmd->transitionImageLayout(
                outputTex->getImage()->getImage(),
                outputTex->getImage()->getFormat(),
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        cmd->transitionImageLayout(
            mAORaw->getImage()->getImage(),
            mAORaw->getImage()->getFormat(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        cmd->transitionImageLayout(
            mEdges->getImage()->getImage(),
            mEdges->getImage()->getFormat(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void GTAOPass::updateBuffer(const DescriptorSet::Ptr& descriptorSet, int frameIndex) {

        mGTAOParam->mBuffers[frameIndex]->updateBufferByMap(&mGTAO, sizeof(GTAOParams));

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = mGTAOParam->mBuffers[frameIndex]->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GTAOParams);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet->getDescriptorSet(frameIndex);
        write.dstBinding = mGTAOParam->mBinding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice->getDevice(), 1, &write, 0, nullptr);
    }
}