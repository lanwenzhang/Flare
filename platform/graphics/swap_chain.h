#pragma once
#include "../../common.h"
#include "../window/window.h"
#include "device.h"
#include "render_pass.h"
#include "image.h"
#include "framebuffer.h"

namespace flare::vk {

	struct SwapChainSupportInfo {
		VkSurfaceCapabilitiesKHR mCapabilities;
		std::vector<VkSurfaceFormatKHR> mFormats;
		std::vector<VkPresentModeKHR> mPresentModes;
	};

	class SwapChain {
	public:
		SwapChain(const Device::Ptr &device, const flare::window::Window::Ptr &window, const Surface::Ptr &surface, const CommandPool::Ptr& commandPool);
		~SwapChain();

		using Ptr = std::shared_ptr<SwapChain>;
		static Ptr create(const Device::Ptr& device, const flare::window::Window::Ptr& window, const Surface::Ptr& surface, const CommandPool::Ptr& commandPool) {
			return std::make_shared<SwapChain>(device, window, surface, commandPool);
		}

		SwapChainSupportInfo querySwapChainSupportInfo();

		VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSurfacePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR & capabilities);

		[[nodiscard]] auto getSwapChain() const { return mSwapChain; }
		[[nodiscard]] auto getExtent() const { return mSwapChainExtent; }
		[[nodiscard]] auto getFormat() const { return mSwapChainFormat; }

		[[nodiscard]] auto getImageCount() const { return mImageCount; }
		[[nodiscard]] auto getImage(uint32_t index) const { return mSwapChainImages[index]; }
		[[nodiscard]] auto getImageView(uint32_t index) const { return mSwapChainImageViews[index]; }

		[[nodiscard]] auto getDepthImage(uint32_t index) const { return mDepthImages[index]->getImage(); }
		[[nodiscard]] auto getDepthImageFormat() const { return mDepthImages[0]->getFormat(); }
		[[nodiscard]] auto getDepthImageView(uint32_t index) const { return mDepthImages[index]->getImageView(); }
	
	private:
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);

	private:
		VkSwapchainKHR mSwapChain{ VK_NULL_HANDLE };
		VkFormat mSwapChainFormat;
		VkExtent2D mSwapChainExtent;

		uint32_t mImageCount{ 0 };
		std::vector<VkImage> mSwapChainImages{};
		std::vector<VkImageView> mSwapChainImageViews{};
		std::vector<Image::Ptr> mDepthImages{};
		std::vector<Image::Ptr> mMultiSampleImages{}; 
		std::vector<Framebuffer::Ptr> mFramebuffers{};

		Device::Ptr mDevice{ nullptr };
		flare::window::Window::Ptr mWindow{ nullptr };
		Surface::Ptr mSurface{ nullptr };
	};
}