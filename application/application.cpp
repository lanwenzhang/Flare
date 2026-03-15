#include "application.h"

namespace flare::app {

	void Application::initWindow() {
		mWindow = Window::create(mWidth, mHeight);
		mWindow->setApp(shared_from_this());

		mCamera.setPosition(glm::vec3(0.0f, 0.5f, 0.0f));
		mCamera.lookAt(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		mCamera.update();
		mCamera.setPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 200.0f);
		mCamera.setSpeed(0.01f);
		mLight.setDirectionFromSpherical(mLightTheta, mLightPhi);
	}

	void Application::initVulkan() {
		mInstance = flare::vk::Instance::create(true);
		mSurface = flare::vk::Surface::create(mInstance, mWindow);
		mDevice = flare::vk::Device::create(mInstance, mSurface);
		mCommandPool = flare::vk::CommandPool::create(mDevice);
		mSwapChain = flare::vk::SwapChain::create(mDevice, mWindow, mSurface, mCommandPool);
		mWidth = mSwapChain->getExtent().width;
		mHeight = mSwapChain->getExtent().height;

		mCommandBuffers.resize(mFramesInFlight);
		for (int i = 0; i < mFramesInFlight; ++i) {
			mCommandBuffers[i] = flare::vk::CommandBuffer::create(mDevice, mCommandPool);
		}
		mImageAvailableSemaphores.resize(mFramesInFlight);
		mRenderFinishedSemaphores.resize(mSwapChain->getImageCount());
		mInFlightFences.resize(mFramesInFlight);

		for (int i = 0; i < mFramesInFlight; ++i) {
			mImageAvailableSemaphores[i] = flare::vk::Semaphore::create(mDevice);
			mInFlightFences[i] = flare::vk::Fence::create(mDevice);
		}
		for (int i = 0; i < mSwapChain->getImageCount(); ++i) {
			mRenderFinishedSemaphores[i] = flare::vk::Semaphore::create(mDevice);
		}
	}

	void Application::initRenderer() {
		mRenderer = flare::renderer::Renderer::create(mDevice, mCommandPool);
		mRenderer->createResources(mFramesInFlight);
		mRenderer->createDescriptorSets(mFramesInFlight);
		mRenderer->createPasses(mSwapChain, mFramesInFlight);
		mRenderer->createFrameGraph();
		mProfiler = flare::renderer::Profiler::create(mDevice);
		mRenderer->setProfiler(mProfiler);
	}

	void Application::initImGui() {
		VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		};

		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000;
		pool_info.poolSizeCount = 1;
		pool_info.pPoolSizes = pool_sizes;

		if (vkCreateDescriptorPool(mDevice->getDevice(), &pool_info, nullptr, &mImGuiDescriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create ImGui descriptor pool");
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		// 2. Platform + Renderer bindings
		ImGui_ImplGlfw_InitForVulkan(mWindow->getWindow(), true);

		// 3. Init info
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = mInstance->getInstance();
		init_info.PhysicalDevice = mDevice->getPhysicalDevice();
		init_info.Device = mDevice->getDevice();
		init_info.QueueFamily = mDevice->getGraphicQueueFamily().value();
		init_info.Queue = mDevice->getGraphicQueue();
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = mImGuiDescriptorPool;
		init_info.MinImageCount = mFramesInFlight;
		init_info.ImageCount = static_cast<uint32_t>(mSwapChain->getImageCount());
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.UseDynamicRendering = true;
		init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		VkFormat color_format = mSwapChain->getFormat();
		init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;
		init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;

		ImGui_ImplVulkan_Init(&init_info);
	}

	void Application::run() {
		initWindow();
		initVulkan();
		initRenderer();
		initImGui();
		while (!mWindow->shouldClose()) {
			mWindow->pollEvents();
			mWindow->processEvent();
			mainLoop();
		}
		vkDeviceWaitIdle(mDevice->getDevice());
		cleanUp();
	}

	void Application::mainLoop() {

		mInFlightFences[mCurrentFrame]->block();
		// 1 prepare
		// 1.1 get image for next frame
		uint32_t imageIndex{ 0 };
		VkResult result = vkAcquireNextImageKHR(mDevice->getDevice(), mSwapChain->getSwapChain(), UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame]->getSemaphore(), VK_NULL_HANDLE, &imageIndex);

		// 1.2 check if recreate swap chain
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Error: failed to acquire next image");
		}

		mProfiler->resolvePreviousFrame(mCurrentFrame);

		 //1.3 update imgui
		if (isAnyImGuiVisible()) {
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// 1.3.1 ui
			if (mShowUI) {
				const float uiWidth = 450.0f;
				const float uiHeight = 500.0f;
				ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(uiWidth, uiHeight), ImGuiCond_Always);
				ImGuiWindowFlags uiFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
				ImGui::Begin("UI");

				if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_OpenOnArrow)) {
					glm::vec3 camPos = mCamera.getPosition();
					ImGui::Text("Position:"); ImGui::SameLine();
					ImGui::Text("X: %.2f", camPos.x); ImGui::SameLine();
					ImGui::Text("Y: %.2f", camPos.y); ImGui::SameLine();
					ImGui::Text("Z: %.2f", camPos.z);
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("View Mode", ImGuiTreeNodeFlags_OpenOnArrow)) {
					int mode = (int)mRenderer->mLightPushConstants.viewMode;
					ImGui::RadioButton("None", &mode, (int)View_None);
					ImGui::RadioButton("Normal", &mode, (int)View_Normal);
					ImGui::RadioButton("Depth", &mode, (int)View_Depth);
					ImGui::RadioButton("Roughness", &mode, (int)View_Roughness);
					ImGui::RadioButton("Metallic", &mode, (int)View_Metallic);
					ImGui::RadioButton("IBL irradiance", &mode, (int)View_IBL_Diffuse);
					ImGui::RadioButton("Visibility", &mode, (int)View_Visibility);
					ImGui::RadioButton("Indirect irradiance", &mode, (int)View_Indirect);
					ImGui::RadioButton("Ambient occlusion", &mode, (int)View_AO);
					mRenderer->mLightPushConstants.viewMode = (uint32_t)mode;
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Culling", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::Checkbox("Enable frustum culling", &mRenderer->mCullingPass->mEnableFrustumCulling);
					ImGui::Checkbox("Enable occlusion culling", &mRenderer->mCullingPass->mEnableOcclusionCulling);
					const int prevFrame = (mCurrentFrame + mFramesInFlight - 1) % mFramesInFlight;
					ImGui::Text("Visible meshes: %u", mRenderer->mCullingPass->getVisibleCount(prevFrame));
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::Text("Directional Light:");
					ImGui::SliderFloat("Theta", &mLightTheta, 0.0f, 360.0f);
					ImGui::SliderFloat("Phi", &mLightPhi, -89.9f, 89.9f);
					ImGui::SliderFloat("Intensity", &mLightParams.intensity, 0.0f, 20.0f);
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Image-based Lighting", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::Checkbox("Enable diffuse IBL", (bool*)&mRenderer->mLightPushConstants.enableDiffuseIBL);
					ImGui::SliderFloat("Diffuse intensity", &mRenderer->mLightPushConstants.diffuseIBLIntensity, 0.0f, 1.0f);
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Global Illumination", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::Checkbox("Enable DDGI", (bool*)&mRenderer->mLightPushConstants.enableDDGI);
					ImGui::SliderFloat("DDGI intensity", &mRenderer->mLightPushConstants.ddgiIntensity, 0.0f, 5.0f);

					ImGui::SeparatorText("Probe setting");
					const auto& ddgi = mRenderer->mDDGIPass->mDDGI;
					ImGui::Text("Probe counts: X = %d, Y = %d, Z = %d", ddgi.probeCounts.x, ddgi.probeCounts.y, ddgi.probeCounts.z);
					ImGui::Text("Total probes: %d", ddgi.probeCounts.w);
					ImGui::Text("Rays per probe: %d", ddgi.probeRays);
					ImGui::Text("Total rays: %llu", uint64_t(ddgi.probeRays) * uint64_t(ddgi.probeCounts.w));

					glm::vec3 probeSpacing(ddgi.probeSpacing.x, ddgi.probeSpacing.y, ddgi.probeSpacing.z);
					if (ImGui::SliderFloat3("Probe spacing", &probeSpacing.x, 0.01f, 2.0f, "%.3f")) {
						mRenderer->mDDGIPass->mDDGI.probeSpacing.x = probeSpacing.x;
						mRenderer->mDDGIPass->mDDGI.probeSpacing.y = probeSpacing.y;
						mRenderer->mDDGIPass->mDDGI.probeSpacing.z = probeSpacing.z;
					}
					float probeScale = ddgi.probeSpacing.w;
					if (ImGui::SliderFloat("Probe scale", &probeScale, 0.01f, 1.0f, "%.3f")) {
						mRenderer->mDDGIPass->mDDGI.probeSpacing.w = probeScale;
					}
					ImGui::Checkbox("Show probes", &mRenderer->mLightingPass->mShowProbe);
					ImGui::Checkbox("Enable probe offset", reinterpret_cast<bool*>(&mRenderer->mDDGIPass->mDDGI.enableProbeOffset));

					ImGui::SeparatorText("Irradiance setting");
					ImGui::Checkbox("Enable backface blending", reinterpret_cast<bool*>(&mRenderer->mDDGIPass->mDDGI.enableBackfaceBlending));
					ImGui::Checkbox("Enable smooth backface", reinterpret_cast<bool*>(&mRenderer->mDDGIPass->mDDGI.enableSmoothBackface));
					ImGui::Checkbox("Enable infinite bounce", reinterpret_cast<bool*>(&mRenderer->mDDGIPass->mDDGI.enableInfiniteBounce));
					ImGui::SliderFloat("Infinite bounce multiplier", &mRenderer->mDDGIPass->mDDGI.infiniteBounceMultiplier, 0.0f, 1.0f);
					ImGui::SliderFloat("Hysteresis", &mRenderer->mDDGIPass->mDDGI.hysteresis, 0.0f, 1.0f);
					ImGui::SliderFloat("Self shadow bias", &mRenderer->mDDGIPass->mDDGI.selfShadowBias, 0.0f, 1.0f);
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::SliderFloat("Radius", &mLightParams.lightRadius, 0.0f, 0.1f);
					static const char* kBlueNoiseItems[] = { "Blue noise 0", "Blue noise 1", "Blue noise 2" };
					ImGui::Combo("Blue noise pattern", reinterpret_cast<int*>(&mRenderer->mRayTracedShadowPass->mBlueNoiseIndex),
						kBlueNoiseItems, IM_ARRAYSIZE(kBlueNoiseItems));
					bool denoiseEnabled = mRenderer->getDenoiseEnabled();
					if (ImGui::Checkbox("Enable SVGF denoise", &denoiseEnabled)) {
						mRenderer->setDenoiseEnabled(denoiseEnabled);
					}
					ImGui::SeparatorText("Temporal filter");
					ImGui::SliderFloat("Normal distance", &mRenderer->mRayTracedShadowPass->mTemporalPC.normalDistance, 0.0f, 1.0f);
					ImGui::SliderFloat("Plane distance", &mRenderer->mRayTracedShadowPass->mTemporalPC.planeDistance, 0.0f, 1.0f);
					ImGui::SliderFloat("Alpha visibility", &mRenderer->mRayTracedShadowPass->mTemporalPC.alphaVis, 0.0f, 1.0f);
					ImGui::SliderFloat("Alpha moment", &mRenderer->mRayTracedShadowPass->mTemporalPC.alphaMoment, 0.0f, 1.0f);

					ImGui::SeparatorText("Spatial filter");
					ImGui::SliderFloat("Phi visibility", &mRenderer->mRayTracedShadowPass->mAtrousPC.phiVisibility, 0.0f, 20.0f);
					ImGui::SliderFloat("Phi normal", &mRenderer->mRayTracedShadowPass->mAtrousPC.phiNormal, 0.0f, 64.0f);
					ImGui::SliderFloat("Sigma depth", &mRenderer->mRayTracedShadowPass->mAtrousPC.sigmaDepth, 0.0f, 2.0f);
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Ambient Occlusion", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::Checkbox("Enable GTAO", (bool*)&mRenderer->mLightPushConstants.enableGTAO);
					ImGui::SliderFloat("Effect radius", &mRenderer->mGTAOPass->mGTAO.effectRadius, 0.0f, 10000.0f);
					ImGui::SliderFloat("Radius multiplier", &mRenderer->mGTAOPass->mGTAO.radiusMultiplier, 0.0f, 3.0f);
					ImGui::SliderFloat("Falloff range", &mRenderer->mGTAOPass->mGTAO.effectFalloffRange, 0.0f, 1.0f);
					ImGui::SliderFloat("Sample distribution power", &mRenderer->mGTAOPass->mGTAO.sampleDistributionPower, 1.0f, 3.0f);
					ImGui::SliderFloat("Thin occluder compensation", &mRenderer->mGTAOPass->mGTAO.thinOccluderCompensation, 0.0f, 0.7f);
					ImGui::SliderFloat("Final power", &mRenderer->mGTAOPass->mGTAO.finalValuePower, 0.5f, 5.0f);
					bool aoDenoiseEnabled = mRenderer->getAODenoiseEnabled();
					if (ImGui::Checkbox("Enable denoise", &aoDenoiseEnabled)) {
						mRenderer->setAODenoiseEnabled(aoDenoiseEnabled);
					}
				}
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Post processing", ImGuiTreeNodeFlags_OpenOnArrow)) {
					ImGui::SeparatorText("Anti-aliasing");
					bool taaEnabled = mRenderer->getTAAEnabled();
					if (ImGui::Checkbox("Enable TAA", &taaEnabled)) {
						mRenderer->setTAAEnabled(taaEnabled);
					}
					ImGui::SeparatorText("Tone mapping");
					ImGui::Text("ACES filmic");
				}
				ImGui::End();
			}
			// 1.3.2 profiler
			if (mShowProfiler) {
				ImGuiIO& io = ImGui::GetIO();
				const float profilerWidth = 450.0f;
				const float profilerHeight = 500.0f;

				ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - profilerWidth, 0.0f), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(profilerWidth, profilerHeight), ImGuiCond_Always);

				ImGuiWindowFlags profilerFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
				ImGui::Begin("Profiler", nullptr, profilerFlags);
				mProfiler->buildUI();
				ImGui::End();
			}
			mLight.setDirectionFromSpherical(mLightTheta, mLightPhi);
			ImGui::Render();
		}
		 //1.4 update per frame descriptor set
		 //1.4.1 update current frame vp
		static uint32_t jitterIndex = 0;
		static uint32_t jitterPeriod = 2;
		auto halton = [](uint32_t i, uint32_t b) {
			float f = 1.0f; float r = 0.0f;
			while (i > 0u) {
				f /= float(b);
				r += f * float(i % b);
				i /= b;
			}
			return r;
		};
		float jx = halton(jitterIndex, 2);
		float jy = halton(jitterIndex, 3);
		jitterIndex = (jitterIndex + 1) % jitterPeriod;
		glm::vec2 pixelOffset = glm::vec2(jx - 0.5f, jy - 0.5f);
		if (mRenderer->getTAAEnabled()) {
			mCamera.setJitter(2.0f * pixelOffset.x / float(mWidth), 2.0f * pixelOffset.y / float(mHeight));
		}
		mRenderer->updateFrameUniform(mCurrentFrame, mCamera);

		// 1.4.2 update current frame taa params
		glm::mat4 currView = mCamera.getViewMatrix();
		glm::mat4 currProj = mCamera.getJitteredProjectionMatrix();
		glm::mat4 currVP = currProj * currView;
		glm::vec2 currPixelOffset = pixelOffset;

		static glm::mat4 prevVP = glm::mat4(1.0f);
		static glm::vec2 prevPixelOffset = glm::vec2(0.0f);
		static bool initialized = false;

		if (!initialized || mResetTAAHistory) {
			prevVP = currVP;
			prevPixelOffset = currPixelOffset;
			initialized = true;
			mResetTAAHistory = false;
		}
		mTAAParams.inverseVP = glm::inverse(currVP);
		mTAAParams.prevVP = prevVP;
		mTAAParams.currPixelOffset = currPixelOffset;
		mTAAParams.prevPixelOffset = prevPixelOffset;
		mTAAParams.resolution = glm::vec2(float(mWidth), float(mHeight));
		mTAAParams.padding = glm::vec2(0.0f);
		mRenderer->updateTAAUniform(mCurrentFrame, mTAAParams);

		prevPixelOffset = currPixelOffset;
		prevVP = currVP; 

		// 1.4.3 update light param
		mLightParams.lightDir = glm::vec4(mLight.getDirection(), 0.0f);
		mRenderer->updateLightUniform(mCurrentFrame, mLightParams);
	
		// 1.5 record commands
		mInFlightFences[mCurrentFrame]->resetFence();
		vkResetCommandBuffer(mCommandBuffers[mCurrentFrame]->getCommandBuffer(), 0);
		render(mCommandBuffers[mCurrentFrame], mFramesInFlight, imageIndex);
		
		// 2 submit to render
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { mImageAvailableSemaphores[mCurrentFrame]->getSemaphore() };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		auto commandBuffer = mCommandBuffers[mCurrentFrame]->getCommandBuffer();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[imageIndex]->getSemaphore() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, mInFlightFences[mCurrentFrame]->getFence()) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to submit renderCommand");
		}

		// 3 submit to present
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { mSwapChain->getSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(mDevice->getPresentQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mWindow->mWindowResized) {
			recreateSwapChain();
			mWindow->mWindowResized = false;
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to present");
		}
		mLightParams.numFrames++;
		mCurrentFrame = (mCurrentFrame + 1) % mFramesInFlight;
	}

	void Application::recreateSwapChain() {
		// 1 check if the window is minimized
		int width = 0, height = 0;
		glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwWaitEvents();
			glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);
		}
		// 2 wait task
		vkDeviceWaitIdle(mDevice->getDevice());

		// 3 clean up
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		vkDestroyDescriptorPool(mDevice->getDevice(), mImGuiDescriptorPool, nullptr);
		mSwapChain.reset();

		// 4 recreate
		mSwapChain = SwapChain::create(mDevice, mWindow, mSurface, mCommandPool);
		mWidth = mSwapChain->getExtent().width;
		mHeight = mSwapChain->getExtent().height;

		mTAAParams.resolution = glm::vec2(float(mWidth), float(mHeight));
		mResetTAAHistory = true;
		initImGui();
	}

	void Application::render(const CommandBuffer::Ptr& cmd, int frameCount, uint32_t imageIndex) {
		cmd->begin();
		if (mProfiler && mProfiler->isEnabled())
			mProfiler->beginFrame(cmd, mCurrentFrame);
			mRenderer->renderScene(mSwapChain, cmd, imageIndex, frameCount, mCurrentFrame, mCamera, mLight);
		if (mProfiler && mProfiler->isEnabled())
			mProfiler->endFrame(cmd, mCurrentFrame);		
		renderUI(cmd, imageIndex);
		cmd->end();
	}

	void Application::renderUI(const CommandBuffer::Ptr& cmd, uint32_t imageIndex) {
		if (!isAnyImGuiVisible()) {
			cmd->transitionImageLayout(
				mSwapChain->getImage(imageIndex),
				mSwapChain->getFormat(),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
			);
			return;
		}

		cmd->beginRenderingForImGui(mSwapChain->getImageView(imageIndex), mSwapChain->getFormat(), mSwapChain->getExtent());

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(mSwapChain->getExtent().width);
		viewport.height = static_cast<float>(mSwapChain->getExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd->setViewport(0, viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = mSwapChain->getExtent();
		cmd->setScissor(0, scissor);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd->getCommandBuffer());
		cmd->endRendering();
		cmd->transitionImageLayout(mSwapChain->getImage(imageIndex), mSwapChain->getFormat(),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}

	void Application::cleanUp() {
		// swap chain
		mSwapChain.reset();

		// imgui
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		vkDestroyDescriptorPool(mDevice->getDevice(), mImGuiDescriptorPool, nullptr);

		// renderer
		mRenderer.reset();

		// vulkan
		mCommandBuffers.clear();
		mImageAvailableSemaphores.clear();
		mRenderFinishedSemaphores.clear();
		mInFlightFences.clear();

		mCommandPool.reset();
		mDevice.reset();
		mSurface.reset();
		mInstance.reset();

		// window
		mWindow.reset();
	}
}