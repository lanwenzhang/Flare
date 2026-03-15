#include "window.h"
#include "../../application/application.h"

namespace flare::window {

	static void windowResized(GLFWwindow* window, int width, int height) {
		auto pUserData = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		pUserData->mWindowResized = true;
	}

	static void cursorPosCallBack(GLFWwindow* window, double xpos, double ypos) {
		auto pUserData = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		auto app = pUserData->mApp;
		if (!app.expired()) {
			auto appReal = app.lock();
			appReal->onMouseMove(xpos, ypos);
		}
	}

	Window::Window(const int& width, const int& height) {
		mWidth = width;
		mHeight = height;

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		mWindow = glfwCreateWindow(mWidth, mHeight, "flare", nullptr, nullptr);
		if (!mWindow) {
			std::cerr << "Error: failed to create window" << std::endl;
		}
		glfwSetWindowUserPointer(mWindow, this);
		glfwSetFramebufferSizeCallback(mWindow, windowResized);
		glfwSetCursorPosCallback(mWindow, cursorPosCallBack);
		glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	Window::~Window() {
		glfwDestroyWindow(mWindow);
		glfwTerminate();
	}

	bool Window::shouldClose() {
		return glfwWindowShouldClose(mWindow);
	}

	void Window::pollEvents() {
		glfwPollEvents();
	}

	void Window::processEvent() {
		if (mApp.expired()) {
			return;
		}
		auto app = mApp.lock();
		if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			exit(0);
		}
		if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS) {
			
			app->onKeyDown(flare::app::CAMERA_MOVE::MOVE_FRONT);
		}
		if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS) {
			app->onKeyDown(flare::app::CAMERA_MOVE::MOVE_BACK);
		}
		if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS) {
			app->onKeyDown(flare::app::CAMERA_MOVE::MOVE_LEFT);
		}

		if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS) {
			app->onKeyDown(flare::app::CAMERA_MOVE::MOVE_RIGHT);
		}
		if (glfwGetMouseButton(mWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			
			app->enableMouseControl(true);
		}
		else {
			app->enableMouseControl(false);
		}
		static bool F1Pressed = false;
		if (glfwGetKey(mWindow, GLFW_KEY_F1) == GLFW_PRESS) {
			if (!F1Pressed) {
				F1Pressed = true;
				auto app = mApp.lock();
				app->toggleUI();
			}
		}
		else {
			F1Pressed = false;
		}
		static bool F2Pressed = false;
		if (glfwGetKey(mWindow, GLFW_KEY_F2) == GLFW_PRESS) {
			if (!F2Pressed) {
				F2Pressed = true;
				auto app = mApp.lock();
				app->toggleProfiler();
			}
		}
		else {
			F2Pressed = false;
		}
	}
}