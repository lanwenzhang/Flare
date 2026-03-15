#include "culling_pass.h"

namespace flare::renderer {

	void CullingPass::init(const DescriptorSetLayout::Ptr& staticSetLayout, const Texture::Ptr& hzbTex, int frameCount) {
		createDescriptor(hzbTex, frameCount);
		createPipeline(staticSetLayout);
	}

	void CullingPass::createDescriptor(const Texture::Ptr& hzbTex, int frameCount) {
		
		std::vector<UniformParameter::Ptr> params;
		mIndirectParam = UniformParameter::create();
		mIndirectParam->mBinding = 0;
		mIndirectParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		mIndirectParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
		mIndirectParam->mCount = 1;
		params.push_back(mIndirectParam);

		mCullingDataParam = UniformParameter::create();
		mCullingDataParam->mBinding = 1;
		mCullingDataParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		mCullingDataParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
		mCullingDataParam->mCount = 1;
		mCullingDataParam->mSize = sizeof(CullingDataCPU);
		for (int i = 0; i < frameCount; ++i) {
			auto buf = Buffer::createHostVisibleStorageBuffer(mDevice, mCullingDataParam->mSize, nullptr);
			mCullingDataParam->mBuffers.push_back(buf);
		}
		params.push_back(mCullingDataParam);

		mHZBParam = UniformParameter::create();
		mHZBParam->mBinding = 2;
		mHZBParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		mHZBParam->mStage = VK_SHADER_STAGE_COMPUTE_BIT;
		mHZBParam->mCount = 1;
		mHZBParam->mImageInfos.resize(1);
		mHZBParam->mImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mHZBParam->mImageInfos[0].imageView = hzbTex->getImageView();
		mHZBParam->mImageInfos[0].sampler = hzbTex->getSampler();
		params.push_back(mHZBParam);

		mDescriptorSetLayout = DescriptorSetLayout::create(mDevice);
		mDescriptorSetLayout->build(params);
		mDescriptorPool = DescriptorPool::create(mDevice);
		mDescriptorPool->build(params, frameCount);
		mDescriptorSet = DescriptorSet::create(mDevice, params, mDescriptorSetLayout, mDescriptorPool, frameCount);

		for (int i = 0; i < frameCount; ++i) {
			mDescriptorSet->updateImage(mDescriptorSet->getDescriptorSet(i), mHZBParam->mBinding, mHZBParam->mImageInfos[0]);
		}
	}

	void CullingPass::createPipeline(const DescriptorSetLayout::Ptr& staticSetLayout) {
		mPipeline = ComputePipeline::create(mDevice);
		auto cullShader = Shader::create(mDevice, "shaders/culling/culling_cs.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main");
		mPipeline->setShader(cullShader);
		mPipeline->setDescriptorSetLayouts({ mDescriptorSetLayout->getLayout(), staticSetLayout->getLayout() });
		mPipeline->build();
	}

	void CullingPass::getFrustumCorners(glm::mat4 viewProj, glm::vec4* points) {
		const glm::vec4 ndc[8] = {
			{-1,-1,0,1},{ 1,-1,0,1},{-1, 1,0,1},{ 1, 1,0,1},
			{-1,-1,1,1},{ 1,-1,1,1},{-1, 1,1,1},{ 1, 1,1,1}
		};
		const glm::mat4 invViewProj = glm::inverse(viewProj);
		for (int i = 0; i < 8; ++i) {
			const glm::vec4 q = invViewProj * ndc[i];
			points[i] = q / q.w;
		}
	}

	void CullingPass::getFrustumPlanes(glm::mat4 viewProj, glm::vec4* planes){
		viewProj = glm::transpose(viewProj);
		planes[0] = glm::vec4(viewProj[3] + viewProj[0]);
		planes[1] = glm::vec4(viewProj[3] - viewProj[0]);
		planes[2] = glm::vec4(viewProj[3] + viewProj[1]);
		planes[3] = glm::vec4(viewProj[3] - viewProj[1]);
		planes[4] = glm::vec4(viewProj[3] + viewProj[2]);
		planes[5] = glm::vec4(viewProj[3] - viewProj[2]);
	}

	void CullingPass::update(const Buffer::Ptr& indirectBuffer, const glm::mat4& view, const glm::mat4& proj, uint32_t numMeshesToCull, 
							int frameIndex){
		CullingDataCPU cd{};
		cd.numMeshesToCull = numMeshesToCull;
		cd.enableFrustumCulling = mEnableFrustumCulling ? 1u : 0u;
		cd.enableOcclusionCulling = mEnableOcclusionCulling ? 1u : 0u;
		cd.proj00 = proj[0][0];
		cd.proj11 = proj[1][1];
		const glm::mat4 VP = proj * view;

		getFrustumPlanes(VP, cd.planes);
		update(indirectBuffer, cd, frameIndex);
	}

	void CullingPass::update(const Buffer::Ptr& indirectBuffer, const CullingDataCPU& cullingData, int frameIndex){
		mCullingDataParam->mBuffers[frameIndex]->updateBufferByMap((void*)&cullingData, sizeof(CullingDataCPU));

		VkDescriptorBufferInfo infos[2]{};
		infos[0].buffer = indirectBuffer->getBuffer();
		infos[0].offset = 0;
		infos[0].range = VK_WHOLE_SIZE;

		infos[1].buffer = mCullingDataParam->mBuffers[frameIndex]->getBuffer();
		infos[1].offset = 0;
		infos[1].range = sizeof(CullingDataCPU);

		VkWriteDescriptorSet writes[2]{};
		for (int i = 0; i < 2; ++i) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = mDescriptorSet->getDescriptorSet(frameIndex);
			writes[i].dstBinding = (i == 0) ? mIndirectParam->mBinding : mCullingDataParam->mBinding;
			writes[i].dstArrayElement = 0;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[i].descriptorCount = 1;
			writes[i].pBufferInfo = &infos[i];
		}
		vkUpdateDescriptorSets(mDevice->getDevice(), 2, writes, 0, nullptr);
	}

	void CullingPass::render(const CommandBuffer::Ptr& cmd, const GeometryBuffer::Ptr& geoBuffer,
									 const DescriptorSet::Ptr& staticSet, Camera camera, int frameIndex) {

		auto indirectBuf = geoBuffer->getIndirectBuffer();
		const glm::mat4 view = camera.getViewMatrix();
		const glm::mat4 proj = camera.getProjectMatrix();
		const uint32_t numMeshesToCull = geoBuffer->getDrawCount();

		update(indirectBuf, view, proj, numMeshesToCull, frameIndex);

		cmd->bindComputePipeline(mPipeline->getPipeline());
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), mDescriptorSet->getDescriptorSet(frameIndex), 0);
		cmd->bindDescriptorSet(VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->getLayout(), staticSet->getDescriptorSet(0), 1);
		const uint32_t groupCountX = (numMeshesToCull + 63) / 64;
		cmd->dispatch(groupCountX, 1, 1);

		VkBufferMemoryBarrier indirectBarrier{};
		indirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		indirectBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		indirectBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		indirectBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		indirectBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		indirectBarrier.buffer = indirectBuf->getBuffer();
		indirectBarrier.offset = 0;
		indirectBarrier.size = VK_WHOLE_SIZE;

		vkCmdPipelineBarrier(cmd->getCommandBuffer(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &indirectBarrier, 0, nullptr);

		VkBufferMemoryBarrier hostBarrier{};
		hostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		hostBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		hostBarrier.buffer = mCullingDataParam->mBuffers[frameIndex]->getBuffer();
		hostBarrier.offset = 0;
		hostBarrier.size = VK_WHOLE_SIZE;

		vkCmdPipelineBarrier(cmd->getCommandBuffer(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &hostBarrier, 0, nullptr);
	}

	uint32_t CullingPass::getVisibleCount(int frameIndex)const {

		uint32_t value = 0;
		const auto& buf = mCullingDataParam->mBuffers[frameIndex];
		void* p = buf->map();
		if (p) {
			const CullingDataCPU* cd = reinterpret_cast<const CullingDataCPU*>(p);
			value = cd->numVisibleMeshes;
			buf->unmap();
		}
		return value;
	}
}