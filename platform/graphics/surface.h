#pragma once

#include "../../common.h"
#include "instance.h"
#include "../window/window.h"

namespace flare::vk {

	using namespace flare::window;

	class Surface {
	public:

		Surface(Instance::Ptr instance, Window::Ptr window);
		~Surface();

		using Ptr = std::shared_ptr<Surface>;
		static Ptr create(Instance::Ptr instance, Window::Ptr window) { return std::make_shared<Surface>(instance, window); }

		[[nodiscard]] auto getSurface() const { return mSurface; }

	private:

		VkSurfaceKHR mSurface{ VK_NULL_HANDLE };
		Instance::Ptr mInstance{ nullptr };

	};

}