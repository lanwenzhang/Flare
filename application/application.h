#pragma once
#include "../common.h"
#include "type.h"
#include "../platform/window/window.h"
#include "../platform/graphics/instance.h"
#include "../platform/graphics/device.h"
#include "../platform/graphics/surface.h"
#include "../platform/graphics/swap_chain.h"
#include "../platform/graphics/command_pool.h"
#include "../platform/graphics/command_buffer.h"
#include "../platform/graphics/semaphore.h"
#include "../platform/graphics/fence.h"

#include "../function/loader/loader.h"
#include "../function/scene/camera/camera.h"
#include "../function/scene/light/directional_light.h"
#include "../function/renderer/renderer.h"
#include "../function/profiler/profiler.h"

#include "../external/imgui/imgui.h"                     
#include "../external/imgui/imgui_impl_vulkan.h"
#include "../external/imgui/imgui_impl_glfw.h"
#include "../external/enkiTS/TaskScheduler.h"

namespace flare::app{
	using namespace flare::vk;
	using namespace flare::window;
	using namespace flare::renderer;

	class Application :public std::enable_shared_from_this<Application> {
	public:
		Application() = default;
		~Application() = default;

		void run();

		void toggleUI() { mShowUI = !mShowUI; }
		void toggleProfiler() { mShowProfiler = !mShowProfiler; }
		bool isAnyImGuiVisible() const { return mShowUI || mShowProfiler; }

		void onMouseMove(double x, double y){ if (!ImGui::GetIO().WantCaptureMouse) { mCamera.onMouseMove(x, y);}}
		void onKeyDown(CAMERA_MOVE direction){ mCamera.move(direction); }
		void enableMouseControl(bool enable){ mCamera.setMouseControl(enable); }
	
	private:
		void initWindow();
		void initVulkan();
		void initRenderer();
		void initImGui();

		void mainLoop();
		void render(const CommandBuffer::Ptr& cmd, int frameCount, uint32_t imageIndex);
		void renderUI(const CommandBuffer::Ptr& cmd, uint32_t imageIndex);
		void recreateSwapChain();
		void cleanUp();

	private:
		unsigned int mWidth{ 1200 };
		unsigned int mHeight{ 800 };
		int mCurrentFrame{ 0 };
		const int mFramesInFlight{ 2 };

		Window::Ptr mWindow{ nullptr };
		enki::TaskScheduler mTaskScheduler{};
		Instance::Ptr mInstance{ nullptr };
		Device::Ptr mDevice{ nullptr };
		Surface::Ptr mSurface{ nullptr };
		CommandPool::Ptr mCommandPool{ nullptr };
		SwapChain::Ptr mSwapChain{ nullptr };
		std::vector<CommandBuffer::Ptr> mCommandBuffers{};
		std::vector<Semaphore::Ptr> mImageAvailableSemaphores{};
		std::vector<Semaphore::Ptr> mRenderFinishedSemaphores{};
		std::vector<Fence::Ptr> mInFlightFences{};
		VkDescriptorPool mImGuiDescriptorPool = VK_NULL_HANDLE;

		Renderer::Ptr mRenderer{ nullptr };
		Camera mCamera;
		DirectionalLight mLight;
		Profiler::Ptr mProfiler;

		VPMatrices mVPMatrices;
		LightParam mLightParams;
		float mLightTheta{ 237.5f };
		float mLightPhi{ -17.8f };
		TAAParams mTAAParams;
		bool      mResetTAAHistory{ true };

		bool mShowUI{ true };
		bool mShowProfiler{ true };
	};
}