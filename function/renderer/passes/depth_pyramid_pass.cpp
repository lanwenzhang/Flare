#include "depth_pyramid_pass.h"

namespace flare::renderer {

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

        VkImageView view{};
        VkResult r = vkCreateImageView(device->getDevice(), &ivci, nullptr, &view);
        if (r != VK_SUCCESS) throw std::runtime_error("DepthPyramidPass: vkCreateImageView failed for mip view");
        return view;
	}

    DepthPyramidPass::~DepthPyramidPass() {
        if (!mDevice) return;

        VkDevice device = mDevice->getDevice();
        for (VkImageView view : mHZBMipViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device, view, nullptr);
            }
        }
        mHZBMipViews.clear();
    }

    void DepthPyramidPass::createImages(const FrameGraph& frameGraph) {
        SamplerDesc pointClamp{};
        pointClamp.minFilter = VK_FILTER_NEAREST;
        pointClamp.magFilter = VK_FILTER_NEAREST;
        pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        pointClamp.anisotropyEnable = VK_FALSE;
        pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        mHZB = Texture::create(mDevice, frameGraph.getImage("hzb"), pointClamp);

        mWidth = static_cast<uint32_t>(mHZB->getImage()->getWidth());
        mHeight = static_cast<uint32_t>(mHZB->getImage()->getHeight());
        mMipCount = mHZB->getImage()->getMipLevels();
    }

    void DepthPyramidPass::init(const FrameGraph& frameGraph, const Texture::Ptr& preDepth, int frameCount) {
        mPreDepth = preDepth;
        createImages(frameGraph);
        createDescriptors(frameCount);
        createPipeline();
    }

    void DepthPyramidPass::createDescriptors(int frameCount){
        mSrcParam = UniformParameter::create();
        mSrcParam->mBinding = 0;
        mSrcParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mSrcParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mSrcParam->mCount = 1;

        mDstParam = UniformParameter::create();
        mDstParam->mBinding = 1;
        mDstParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        mDstParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
        mDstParam->mCount = 1;

        std::vector<UniformParameter::Ptr> params{ mSrcParam, mDstParam };

        mSetLayout = DescriptorSetLayout::create(mDevice);
        mSetLayout->build(params);

        mDescriptorPool = DescriptorPool::create(mDevice);
        mDescriptorPool->build(params, frameCount * static_cast<int>(mMipCount));

        mHZBMipViews.resize(mMipCount);
        mDescriptorSets.resize(mMipCount);

        VkImage  hzbImage = mHZB->getImage()->getImage();
        VkFormat hzbFmt = mHZB->getImage()->getFormat();

        for (uint32_t mip = 0; mip < mMipCount; ++mip) {
            mHZBMipViews[mip] = createViewForMipmap(mDevice, hzbImage, hzbFmt, mip);
            mDescriptorSets[mip] = DescriptorSet::create(mDevice, params, mSetLayout, mDescriptorPool, frameCount);

            for (int frame = 0; frame < frameCount; ++frame) {
                VkDescriptorSet vkSet = mDescriptorSets[mip]->getDescriptorSet(frame);
                VkDescriptorImageInfo srcInfo{};
                if (mip == 0) {
                    srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; 
                    srcInfo.imageView = mPreDepth->getImageView();
                    srcInfo.sampler = mPreDepth->getSampler();
                }
                else {
                    srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    srcInfo.imageView = mHZBMipViews[mip - 1];
                    srcInfo.sampler = mHZB->getSampler();
                }
                VkDescriptorImageInfo dstInfo{};
                dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                dstInfo.imageView = mHZBMipViews[mip];
                dstInfo.sampler = VK_NULL_HANDLE;

                mDescriptorSets[mip]->updateImage(vkSet, mSrcParam->mBinding, srcInfo);
                mDescriptorSets[mip]->updateStorageImage(vkSet, mDstParam->mBinding, dstInfo);
            }
        }
    }

    void DepthPyramidPass::createPipeline(){
        mPipeline = ComputePipeline::create(mDevice);
        auto cs = Shader::create(mDevice, "shaders/depth_pyramid/depth_pyramid_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
        mPipeline->setShader(cs);
        mPipeline->setDescriptorSetLayouts({ mSetLayout->getLayout() });
        mPipeline->build();
    }

    void DepthPyramidPass::render(const CommandBuffer::Ptr& cmd, int frameIndex){
        auto pyrImg = mHZB->getImage()->getImage();
        auto pyrFmt = mHZB->getImage()->getFormat();
    
        cmd->bindComputePipeline(mPipeline->getPipeline());

        uint32_t width = mWidth;
        uint32_t height = mHeight;

        for (uint32_t mip = 0; mip < mMipCount; ++mip) {
            cmd->transitionImageLayout(pyrImg, pyrFmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, mip, 1);

            cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(),
                                   mDescriptorSets[mip]->getDescriptorSet(frameIndex), 0);

            uint32_t groupX = (width + 7) / 8;
            uint32_t groupY = (height + 7) / 8;
            cmd->dispatch(groupX, groupY, 1);

            VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.image = pyrImg;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = mip;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd->getCommandBuffer(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            
            width = width > 1 ? (width / 2) : 1;
            height = height > 1 ? (height / 2) : 1;
        }

        cmd->transitionImageLayout(pyrImg, pyrFmt, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                   0, mMipCount);

    }

}