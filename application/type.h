#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace flare::app {

	enum class CAMERA_MOVE {
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_FRONT,
		MOVE_BACK
	};

	struct VPMatrices {
		glm::mat4 mViewMatrix;
		glm::mat4 mProjectionMatrix;
	};

	struct LightParam {
		glm::vec4 lightDir;
		float intensity{ 10.0f };
		float lightRadius{ 0.006f };
		uint32_t numFrames{ 0 };
		uint32_t padding;
	};

	enum ViewMode : uint32_t {
		View_None = 0,
		View_Normal = 1,
		View_Depth = 2,
		View_Roughness = 3,
		View_Metallic = 4,
		View_IBL_Diffuse = 5,
		View_Visibility = 6,
		View_Indirect = 7,
		View_AO = 8,
	};

	struct LightPushConstant {
		glm::vec4 cameraPos;
		float near = 0.1f;
		float far = 200.0f;
		uint32_t enableDiffuseIBL = 0u;
		float diffuseIBLIntensity = 0.0f;
		uint32_t enableGTAO = 1u;
		uint32_t enableDDGI = 1u;
		float ddgiIntensity = 0.529f;
		uint32_t viewMode = View_None;
	};

	struct ShadowTemporalPC {
		float normalDistance = 0.1f;
		float planeDistance = 0.3f;
		float alphaVis = 0.4f;
		float alphaMoment = 0.2f;
	};

	 struct ShadowAtrousPC {
		 int radius{ 1 };
		 int stepSize;
		 float phiVisibility{ 4.0f };
		 float phiNormal{ 32.0f };
		 float sigmaDepth{ 1.0f };
		 float power{ 0.8f };
	};

	 struct TAAParams {
		 glm::mat4 inverseVP;
		 glm::mat4 prevVP;
		 glm::vec2 currPixelOffset;
		 glm::vec2 prevPixelOffset;
		 glm::vec2 resolution;
		 glm::vec2 padding;
	 };
}


