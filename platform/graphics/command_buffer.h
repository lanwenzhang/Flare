#pragma once

#include "../../common.h"
#include "../../application/type.h"
#include "device.h"
#include "command_pool.h"
#include "swap_chain.h"
#include "framebuffer.h"

namespace flare::vk {

	class CommandBuffer {
	public:
		using Ptr = std::shared_ptr<CommandBuffer>;
		static Ptr create(const Device::Ptr& device, const CommandPool::Ptr& commandPool, bool asSecondary = false) { 
			return std::make_shared<CommandBuffer>(device, commandPool, asSecondary); 
		}
		
		CommandBuffer(const Device::Ptr& device, const CommandPool::Ptr& commandPool, bool asSecondary = false);
		~CommandBuffer();

		void begin(VkCommandBufferUsageFlags flag = 0, const VkCommandBufferInheritanceInfo& inheritance = {});
		void beginRendering(const Framebuffer::Ptr& framebuffer, VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
							VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);
		void beginRendering(const Framebuffer::Ptr& framebuffer, uint32_t layer);
		void beginRendering(const SwapChain::Ptr& swapchain, uint32_t imageIndex);
		void beginRenderingForImGui(VkImageView colorImageView, VkFormat colorFormat, VkExtent2D extent);
		void beginRenderPass(const VkRenderPassBeginInfo& renderPassBeginInfo, const VkSubpassContents &subPassContents = VK_SUBPASS_CONTENTS_INLINE);

		void setViewport(uint32_t firstViewport, const VkViewport& viewport);
		void setScissor(uint32_t firstScissor, const VkRect2D& scissor);
		void setDepthBias(float constant, float clamp, float slope);
		void disableDepthBias();

		void bindGraphicPipeline(const VkPipeline& pipeline);
		void bindComputePipeline(const VkPipeline& pipeline);
		void bindRayTracingPipeline(const VkPipeline& pipeline);
		void bindVertexBuffer(const std::vector<VkBuffer>& buffers);
		void bindIndexBuffer(const VkBuffer& buffer);
		void bindDescriptorSet(VkPipelineBindPoint bindPoint, const VkPipelineLayout layout, const VkDescriptorSet& descriptorSet, uint32_t setIndex);
		void pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const flare::app::LightPushConstant& pc);
		void pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const flare::app::ShadowTemporalPC& pc);
		void pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const flare::app::ShadowAtrousPC& pc);
		void pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const glm::vec4& pc);
		void pushConstants(const VkPipelineLayout layout, VkShaderStageFlags stageFlags, const uint32_t& pc);

		void dispatch(uint32_t x, uint32_t y, uint32_t z);

		void draw(size_t vertexCount);
		void drawIndex(size_t indexCount);
		void drawIndexInstanced(uint32_t indexCount, uint32_t instancingCount);
		void drawIndexedIndirect(VkBuffer indirectBuffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride);

		void endRenderPass();
		void endRendering();
		void end();

		void resolveDepthImage(VkImage srcImage, VkImage dstImage, VkFormat depthFormat, uint32_t width,uint32_t height);
		void copyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t copyInfoCount, const std::vector<VkBufferCopy>& copyInfos);
		void copyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, uint32_t arrayLayer = 0);
		void copyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
			           uint32_t width, uint32_t height, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t layerCount = 1);
		void submitSync(VkQueue queue, VkFence fence = VK_NULL_HANDLE);

		void transferImageLayout(const VkImageMemoryBarrier &imageMemoryBarrier, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);
		void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
								   VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, uint32_t baseMipLevel = 0, 
								   uint32_t mipLevelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);

		VkImageAspectFlags getAspectMaskForFormat(VkFormat format);

		[[nodiscard]] auto getCommandBuffer()const { return mCommandBuffer; }

	private:
		VkCommandBuffer mCommandBuffer{ VK_NULL_HANDLE };
		Device::Ptr mDevice{ nullptr };
		CommandPool::Ptr mCommandPool{ nullptr };
	};
}