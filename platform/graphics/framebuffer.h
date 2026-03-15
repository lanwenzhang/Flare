#pragma once

#include "device.h"
#include "image.h"
#include "render_pass.h"

namespace flare::vk {

	class Framebuffer {
	public:

		using Ptr = std::shared_ptr<Framebuffer>;
		static Ptr create(const Device::Ptr& device, const uint32_t& width, const uint32_t& height, bool dynamic) {
			return std::make_shared<Framebuffer>(device, width, height, dynamic);
		}

		Framebuffer(const Device::Ptr& device, const uint32_t& width, const uint32_t& height, bool dynamic);
		~Framebuffer();

		void setRenderPass(const RenderPass::Ptr& renderPass);
		void addAttachment(const Image::Ptr& image);
		void addColorAttachment(const Image::Ptr& image);
		void addColorAttachmentView(const Image::Ptr& image, uint32_t layer);
		void addResolveAttachment(const Image::Ptr& image);
		void addDepthAttachment(const Image::Ptr& image);
		void addDepthResolveAttachment(const Image::Ptr& image);

		void build();

		[[nodiscard]] auto getFramebuffer() const { return mFramebuffer; }
		[[nodiscard]] auto& getDevice() const { return mDevice; }
		[[nodiscard]] auto getWidth() const { return mWidth; }
		[[nodiscard]] auto getHeight() const { return mHeight; }

		[[nodiscard]] const auto& getColorAttachments() const { return mColorAttachments; }
		[[nodiscard]] const auto& getResolveAttachments() const { return mResolveAttachments; }
		[[nodiscard]] const auto& getDepthAttachment() const { return mDepthAttachment; }
		[[nodiscard]] const auto& getDepthResolveAttachment() const { return mDepthResolveAttachment; }

		[[nodiscard]] const auto& Framebuffer::getColorAttachmentView(uint32_t layer) const { return mColorAttachmentViews[layer];}

	private:

		Device::Ptr mDevice{ nullptr };
		bool mUseDynamicRendering = false;
		RenderPass::Ptr mRenderPass{ nullptr };
		VkFramebuffer mFramebuffer{ VK_NULL_HANDLE };
		uint32_t mWidth{ 1920 };
		uint32_t mHeight{ 1080 };
		std::vector<Image::Ptr> mAttachments {};
		std::vector<Image::Ptr> mColorAttachments{};
		std::vector<Image::Ptr> mResolveAttachments{};
		Image::Ptr mDepthAttachment{ nullptr };
		Image::Ptr mDepthResolveAttachment{ nullptr };
		std::vector<VkImageView> mColorAttachmentViews{};
	};
}