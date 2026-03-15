#include "framebuffer.h"

namespace flare::vk {

	Framebuffer::Framebuffer(const Device::Ptr& device, const uint32_t& width, const uint32_t& height, bool dynamic){
		mDevice = device;
		mWidth = width;
		mHeight = height;
		mUseDynamicRendering = dynamic;
	}

	Framebuffer::~Framebuffer() {
		if (mFramebuffer != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(mDevice->getDevice(), mFramebuffer, nullptr);
			mFramebuffer = VK_NULL_HANDLE;
		}
	
	}

	void Framebuffer::setRenderPass(const RenderPass::Ptr& renderPass) {
		mRenderPass = renderPass;
	}

	void Framebuffer::addAttachment(const Image::Ptr& image) {

		mAttachments.push_back(image);
	}

	void Framebuffer::addColorAttachment(const Image::Ptr& image) {
		
		mColorAttachments.push_back(image);
	}

	void Framebuffer::addColorAttachmentView(const Image::Ptr& image, uint32_t layer) {
		VkImageView layerView = image->createViewForLayer(layer);
		mColorAttachmentViews.push_back(layerView);
	}

	void Framebuffer::addResolveAttachment(const Image::Ptr& image) {
		mResolveAttachments.push_back(image);
	}

	void Framebuffer::addDepthAttachment(const Image::Ptr& image) {
		mDepthAttachment = image;
	}

	void Framebuffer::addDepthResolveAttachment(const Image::Ptr& image) {
		mDepthResolveAttachment = image;
	}

	void Framebuffer::build() {

		if (mUseDynamicRendering) {
			return;
		}

		std::vector<VkImageView> views;

		// 1. Color resolve attachments
		for (const auto& image : mResolveAttachments) {
			views.push_back(image->getImageView());
		}

		// 2. MSAA color attachments
		for (const auto& image : mColorAttachments) {
			views.push_back(image->getImageView());
		}

		// 3. Depth resolve attachment
		if (mDepthResolveAttachment) {
			views.push_back(mDepthResolveAttachment->getImageView());
		}

		// 4. MSAA Depth attachment
		if (mDepthAttachment) {
			views.push_back(mDepthAttachment->getImageView());
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = mRenderPass->getRenderPass();
		framebufferInfo.attachmentCount = static_cast<uint32_t>(views.size());
		framebufferInfo.pAttachments = views.data();
		framebufferInfo.width = mWidth;
		framebufferInfo.height = mHeight;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(mDevice->getDevice(), &framebufferInfo, nullptr, &mFramebuffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer.");
		}
	}

}