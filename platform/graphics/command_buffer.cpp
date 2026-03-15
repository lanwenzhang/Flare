#include "command_buffer.h"
#include "render_pass.h"

namespace flare::vk {

	CommandBuffer::CommandBuffer(const Device::Ptr& device, const CommandPool::Ptr& commandPool, bool asSecondary){
		
		mDevice = device;
		mCommandPool = commandPool;

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = mCommandPool->getCommandPool();
		allocInfo.level = asSecondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(mDevice->getDevice(), &allocInfo, &mCommandBuffer) != VK_SUCCESS) {

			throw std::runtime_error("Error: failed to create command buffer");
		}
	}
	
	CommandBuffer::~CommandBuffer() {

		if (mCommandBuffer != VK_NULL_HANDLE) {

			vkFreeCommandBuffers(mDevice->getDevice(), mCommandPool->getCommandPool(), 1, &mCommandBuffer);
		}
	}

	void CommandBuffer::begin(VkCommandBufferUsageFlags flag, const VkCommandBufferInheritanceInfo& inheritance){
	
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = flag;
		beginInfo.pInheritanceInfo = &inheritance;

		if (vkBeginCommandBuffer(mCommandBuffer, &beginInfo) != VK_SUCCESS) {

			throw std::runtime_error("Error: failed to begin command buffer");
		};
	}

	void CommandBuffer::beginRendering(const Framebuffer::Ptr& framebuffer, VkAttachmentLoadOp colorLoadOp, VkAttachmentLoadOp depthLoadOp) {

		std::vector<VkRenderingAttachmentInfo> colorAttachments;
		const auto& colorAttachmentsFB = framebuffer->getColorAttachments();
		uint32_t colorAttachmentCount = 0;
		const VkRenderingAttachmentInfo* pColorAttachmentInfos = nullptr;

		if (!colorAttachmentsFB.empty()) {
			for (size_t i = 0; i < colorAttachmentsFB.size(); ++i) {
				auto& image = colorAttachmentsFB[i];
				if (!image) continue;

				VkRenderingAttachmentInfo attachmentInfo{};
				attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				attachmentInfo.imageView = image->getImageView();
				attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attachmentInfo.loadOp = colorLoadOp;
				attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
				attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
				colorAttachments.push_back(attachmentInfo);
			}

			colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
			if (colorAttachmentCount > 0) {
				pColorAttachmentInfos = colorAttachments.data();
			}
		}

		// --- Depth Attachment ---
		VkRenderingAttachmentInfo depthAttachment{};
		const VkRenderingAttachmentInfo* pDepthAttachment = nullptr;

		auto depthImage = framebuffer->getDepthAttachment();
		if (depthImage) {
			depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.imageView = depthImage->getImageView();
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp = depthLoadOp;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
			depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
			pDepthAttachment = &depthAttachment;
		}

		// --- Begin Rendering ---
		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea = { {0, 0}, {framebuffer->getWidth(), framebuffer->getHeight()} };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = colorAttachmentCount;
		renderingInfo.pColorAttachments = pColorAttachmentInfos;
		renderingInfo.pDepthAttachment = pDepthAttachment;
		renderingInfo.pStencilAttachment = nullptr;

		vkCmdBeginRendering(mCommandBuffer, &renderingInfo);
	}

	void CommandBuffer::beginRendering(const Framebuffer::Ptr& framebuffer, uint32_t layer) {
		
		std::vector<VkRenderingAttachmentInfo> colorAttachments;

		const auto& colorAttachmentsFB = framebuffer->getColorAttachments();
		uint32_t colorAttachmentCount = 0;
		const VkRenderingAttachmentInfo* pColorAttachmentInfos = nullptr;

		if (!colorAttachmentsFB.empty()) {

			for (size_t i = 0; i < colorAttachmentsFB.size(); ++i) {
				
				auto& image = colorAttachmentsFB[i];
				if (!image) continue;

				VkRenderingAttachmentInfo attachmentInfo{};
				attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				attachmentInfo.imageView = framebuffer->getColorAttachmentView(layer);
				attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
				attachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
				colorAttachments.push_back(attachmentInfo);
			}

			colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
			if (colorAttachmentCount > 0) {
				pColorAttachmentInfos = colorAttachments.data();
			}
		}

		// --- Depth Attachment ---
		VkRenderingAttachmentInfo depthAttachment{};
		const VkRenderingAttachmentInfo* pDepthAttachment = nullptr;

		auto depthImage = framebuffer->getDepthAttachment();
		if (depthImage) {
			depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.imageView = depthImage->getImageView();
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
			depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
			pDepthAttachment = &depthAttachment;
		}

		// --- Begin Rendering ---
		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea = { {0, 0}, {framebuffer->getWidth(), framebuffer->getHeight()} };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = colorAttachmentCount;
		renderingInfo.pColorAttachments = pColorAttachmentInfos;
		renderingInfo.pDepthAttachment = pDepthAttachment;
		renderingInfo.pStencilAttachment = nullptr;

		vkCmdBeginRendering(mCommandBuffer, &renderingInfo);
	}

	void CommandBuffer::beginRendering(const SwapChain::Ptr& swapchain, uint32_t imageIndex) {
		
		VkImageView colorImageView = swapchain->getImageView(imageIndex);
		VkExtent2D extent = swapchain->getExtent();

		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = colorImageView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;

		// Optional depth attachment
		VkImageView depthImageView = swapchain->getDepthImageView(imageIndex);
		VkRenderingAttachmentInfo depthAttachment{};
		VkRenderingAttachmentInfo* pDepthAttachment = nullptr;

		if (depthImageView != VK_NULL_HANDLE) {
			depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.imageView = depthImageView;
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
			depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;

			pDepthAttachment = &depthAttachment;
		}

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea = { {0, 0}, extent };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		renderingInfo.pDepthAttachment = pDepthAttachment;

		vkCmdBeginRendering(mCommandBuffer, &renderingInfo);
	}

	void CommandBuffer::beginRenderingForImGui(VkImageView colorImageView, VkFormat colorFormat, VkExtent2D extent) {
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = colorImageView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color = { {0.f, 0.f, 0.f, 1.f} };
		colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.renderArea = { {0, 0}, extent };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;
		renderingInfo.pDepthAttachment = nullptr;
		renderingInfo.pStencilAttachment = nullptr;

		vkCmdBeginRendering(mCommandBuffer, &renderingInfo);
	}

	void CommandBuffer::beginRenderPass(const VkRenderPassBeginInfo& renderPassBeginInfo, const VkSubpassContents& subPassContents){
	
		vkCmdBeginRenderPass(mCommandBuffer, &renderPassBeginInfo, subPassContents);
	
	}

	void CommandBuffer::setViewport(uint32_t firstViewport, const VkViewport& viewport){
		
		vkCmdSetViewport(mCommandBuffer, firstViewport, 1, &viewport);
	}

	void CommandBuffer::setScissor(uint32_t firstScissor, const VkRect2D& scissor){
		vkCmdSetScissor(mCommandBuffer, firstScissor, 1, &scissor);
	}

	void CommandBuffer::setDepthBias(float constant, float clamp, float slope) {
		vkCmdSetDepthBias(mCommandBuffer, constant, clamp, slope);
	}

	void CommandBuffer::disableDepthBias() {
		vkCmdSetDepthBias(mCommandBuffer, 0.0f, 0.0f, 0.0f);
	}

	void CommandBuffer::bindGraphicPipeline(const VkPipeline& pipeline){
		vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}

	void CommandBuffer::bindComputePipeline(const VkPipeline& pipeline) {
		vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	}

	void CommandBuffer::bindRayTracingPipeline(const VkPipeline& pipeline) {
		vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	}

	void CommandBuffer::pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const flare::app::LightPushConstant& pc) {
		vkCmdPushConstants(mCommandBuffer, layout, stageFlags, 0, sizeof(flare::app::LightPushConstant), &pc);
	}

	void CommandBuffer::pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const flare::app::ShadowTemporalPC& pc) {
		vkCmdPushConstants(mCommandBuffer, layout, stageFlags, 0, sizeof(flare::app::ShadowTemporalPC), &pc);
	}

	void CommandBuffer::pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const flare::app::ShadowAtrousPC& pc) {
		vkCmdPushConstants(mCommandBuffer, layout, stageFlags, 0, sizeof(flare::app::ShadowAtrousPC), &pc);
	}

	void CommandBuffer::pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const glm::vec4& pc){
		vkCmdPushConstants(mCommandBuffer, layout, stageFlags, 0, sizeof(glm::vec4), &pc);
	}

	void CommandBuffer::pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const uint32_t& pc) {
		vkCmdPushConstants(mCommandBuffer, layout, stageFlags, 0, sizeof(uint32_t), &pc);
	}

	void CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) {
		vkCmdDispatch(mCommandBuffer, x, y, z);
	}

	void CommandBuffer::bindVertexBuffer(const std::vector<VkBuffer>& buffers) {
		std::vector<VkDeviceSize> offsets(buffers.size(), 0);
		vkCmdBindVertexBuffers(mCommandBuffer, 0, static_cast<uint32_t>(buffers.size()), buffers.data(), offsets.data());
	}

	void CommandBuffer::bindIndexBuffer(const VkBuffer& buffer) {
		vkCmdBindIndexBuffer(mCommandBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	void CommandBuffer::bindDescriptorSet(VkPipelineBindPoint bindPoint, const VkPipelineLayout layout, const VkDescriptorSet& descriptorSet, uint32_t setIndex) {

		vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, layout, setIndex, 1, &descriptorSet, 0, nullptr);
	}

	void CommandBuffer::draw(size_t vertexCount){
	
		vkCmdDraw(mCommandBuffer, vertexCount, 1, 0, 0);
	}

	void CommandBuffer::drawIndex(size_t indexCount) {
		vkCmdDrawIndexed(mCommandBuffer, indexCount, 1, 0, 0, 0);
	}

	void CommandBuffer::drawIndexInstanced(uint32_t indexCount, uint32_t instancingCount) {
		vkCmdDrawIndexed(mCommandBuffer, indexCount, instancingCount, 0, 0, 0);
	}

	void CommandBuffer::drawIndexedIndirect(VkBuffer indirectBuffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
		
		vkCmdDrawIndexedIndirect(mCommandBuffer, indirectBuffer, offset, drawCount, stride);
	}

	void CommandBuffer::endRenderPass(){
	
		vkCmdEndRenderPass(mCommandBuffer);
	}

	void CommandBuffer::endRendering() {
		
		vkCmdEndRendering(mCommandBuffer);
	}

	void CommandBuffer::end(){
	
		if (vkEndCommandBuffer(mCommandBuffer) != VK_SUCCESS) {

			throw std::runtime_error("Error: failed to end command buffer");
		}
	}

	void CommandBuffer::resolveDepthImage(
		VkImage srcImage,
		VkImage dstImage,
		VkFormat depthFormat,
		uint32_t width,
		uint32_t height
	) {
		// 1. Transition src (MSAA depth) to TRANSFER_SRC
		transitionImageLayout(
			srcImage,
			depthFormat,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		// 2. Transition dst (non-MSAA depth) to TRANSFER_DST
		transitionImageLayout(
			dstImage,
			depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		// 3. Resolve
		VkImageResolve resolveRegion{};
		resolveRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		resolveRegion.srcSubresource.mipLevel = 0;
		resolveRegion.srcSubresource.baseArrayLayer = 0;
		resolveRegion.srcSubresource.layerCount = 1;
		resolveRegion.srcOffset = { 0, 0, 0 };

		resolveRegion.dstSubresource = resolveRegion.srcSubresource;
		resolveRegion.dstOffset = { 0, 0, 0 };
		resolveRegion.extent = { width, height, 1 };

		vkCmdResolveImage(
			mCommandBuffer,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &resolveRegion
		);

		// 4. Transition dst to SHADER_READ_ONLY for SSAO
		transitionImageLayout(
			dstImage,
			depthFormat,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		);
	}


	void CommandBuffer::copyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t copyInfoCount, const std::vector<VkBufferCopy>& copyInfos) {
		
		vkCmdCopyBuffer(mCommandBuffer, srcBuffer, dstBuffer, copyInfoCount, copyInfos.data());
	}

	void CommandBuffer::copyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, uint32_t arrayLayer) {

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = arrayLayer;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(mCommandBuffer, srcBuffer, dstImage, dstImageLayout, 1, &region);

	}

	void CommandBuffer::copyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
								 uint32_t width, uint32_t height, VkImageAspectFlags aspectMask, uint32_t layerCount) {
		VkImageCopy region{};
		region.srcSubresource.aspectMask = aspectMask;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = layerCount;
		region.srcOffset = { 0, 0, 0 };

		region.dstSubresource.aspectMask = aspectMask;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = layerCount;
		region.dstOffset = { 0, 0, 0 };

		region.extent = { width, height, 1 };

		vkCmdCopyImage(mCommandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, 1, &region);
	}


	void CommandBuffer::submitSync(VkQueue queue, VkFence fence) {
		
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mCommandBuffer;

		vkQueueSubmit(queue, 1, &submitInfo, fence);

		vkQueueWaitIdle(queue);
	}

	void CommandBuffer::transferImageLayout(const VkImageMemoryBarrier &imageMemoryBarrier, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {

		vkCmdPipelineBarrier(mCommandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	}

	VkImageAspectFlags CommandBuffer::getAspectMaskForFormat(VkFormat format) {
		if (format == VK_FORMAT_D32_SFLOAT ||
			format == VK_FORMAT_D16_UNORM) {
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else if (format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT) {
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else {
			return VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}

	void CommandBuffer::transitionImageLayout(
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkPipelineStageFlags srcStage,
		VkPipelineStageFlags dstStage,
		uint32_t baseMipLevel,
		uint32_t mipLevelCount,
		uint32_t baseArrayLayer,
		uint32_t layerCount
	) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;

		//
		barrier.subresourceRange.aspectMask = getAspectMaskForFormat(format);
		barrier.subresourceRange.baseMipLevel = baseMipLevel;
		barrier.subresourceRange.levelCount = mipLevelCount;
		barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
		barrier.subresourceRange.layerCount = layerCount;

		// src access mask
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
			barrier.srcAccessMask = 0;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		// dst access mask
		if (newLayout == VK_IMAGE_LAYOUT_GENERAL) {
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		transferImageLayout(barrier, srcStage, dstStage);
	}


}