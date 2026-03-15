#include "motion_vector_pass.h"

namespace flare::renderer {

	void MotionVectorPass::init(const Texture::Ptr& depthTex, const FrameGraph& frameGraph, const DescriptorSetLayout::Ptr& frameSetLayout, int frameCount) {
		createImages(frameGraph);
		createDescriptor(depthTex, frameCount);
		createPipeline(frameSetLayout);
	}

	void MotionVectorPass::createImages(const FrameGraph& frameGraph) {

		SamplerDesc pointClamp{};
		pointClamp.minFilter = VK_FILTER_NEAREST;
		pointClamp.magFilter = VK_FILTER_NEAREST;
		pointClamp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		pointClamp.anisotropyEnable = VK_FALSE;
		pointClamp.addressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pointClamp.addressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		mMotionVectors = Texture::create(mDevice, frameGraph.getImage("motion_vectors"), pointClamp);
	}

	void MotionVectorPass::createDescriptor(const Texture::Ptr& depthTex, int frameCount) {
		std::vector<UniformParameter::Ptr> params;
		mDepthParam = UniformParameter::create(); 
		mDepthParam->mBinding = 0;
		mDepthParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mDepthParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
		mDepthParam->mCount = 1;
		mDepthParam->mImageInfos.resize(1);
		mDepthParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mDepthParam->mImageInfos[0].imageView = depthTex->getImageView();
		mDepthParam->mImageInfos[0].sampler = depthTex->getSampler();
		params.push_back(mDepthParam);

		mMotionVectorParam = UniformParameter::create();
		mMotionVectorParam->mBinding = 1;
		mMotionVectorParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		mMotionVectorParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
		mMotionVectorParam->mCount = 1;
		mMotionVectorParam->mImageInfos.resize(1);
		mMotionVectorParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		mMotionVectorParam->mImageInfos[0].imageView = mMotionVectors->getImageView();
		mMotionVectorParam->mImageInfos[0].sampler = VK_NULL_HANDLE;
		params.push_back(mMotionVectorParam);

		mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout->build(params);
		mDescriptorPool = DescriptorPool::create(mDevice);
		mDescriptorPool->build(params, frameCount);
		mDescriptorSet = DescriptorSet::create(mDevice, params, mDescriptorSetLayout, mDescriptorPool, frameCount);

		for (int i = 0; i < frameCount; ++i) {
			mDescriptorSet->updateImage(mDescriptorSet->getDescriptorSet(i), mDepthParam->mBinding, mDepthParam->mImageInfos[0]);
			mDescriptorSet->updateStorageImage(mDescriptorSet->getDescriptorSet(i), mMotionVectorParam->mBinding, mMotionVectorParam->mImageInfos[0]);
		}
	}

	void MotionVectorPass::createPipeline(const DescriptorSetLayout::Ptr& frameSetLayout) {
		mPipeline = ComputePipeline::create(mDevice);
		auto cullShader = Shader::create(mDevice, "shaders/motion_vector/motion_vector_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mPipeline->setShader(cullShader);
		mPipeline->setDescriptorSetLayouts({ frameSetLayout->getLayout(), mDescriptorSetLayout->getLayout() });
		mPipeline->build();
	}

	void MotionVectorPass::render(const CommandBuffer::Ptr& cmd, const DescriptorSet::Ptr& frameSet, int frameIndex) {

		cmd->transitionImageLayout(mMotionVectors->getImage()->getImage(), mMotionVectors->getImage()->getFormat(),
								   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		cmd->bindComputePipeline(mPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), frameSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 1);

		const uint32_t groupX = (mWidth + 7) / 8;
		const uint32_t groupY = (mHeight + 7) / 8;
		cmd->dispatch(groupX, groupY, 1);

		cmd->transitionImageLayout(mMotionVectors->getImage()->getImage(), mMotionVectors->getImage()->getFormat(),
								   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}
}