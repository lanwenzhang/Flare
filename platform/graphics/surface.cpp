#include "surface.h"

namespace flare::vk {

	Surface::Surface(Instance::Ptr instance, flare::window::Window::Ptr window) {

		mInstance = instance;

		if (glfwCreateWindowSurface(instance->getInstance(), window->getWindow(), nullptr, &mSurface) != VK_SUCCESS) {

			throw std::runtime_error("Error: failed to create surface");
		}
	}

	Surface::~Surface() {

		vkDestroySurfaceKHR(mInstance->getInstance(), mSurface, nullptr);
		mInstance.reset();
	}
}