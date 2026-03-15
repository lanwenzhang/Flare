#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace flare::renderer {

	class DirectionalLight {
	public:

		DirectionalLight();
		~DirectionalLight();

		void setDirectionFromSpherical(float theta, float phi);
		glm::vec3 getDirection() const;
		glm::mat4 getViewMatrix() const;
		glm::mat4 getProjectionMatrix(float left, float right, float bottom, float top, float zNear, float zFar)const;

		void setColor(const glm::vec3& color) { mColor = color; }
		void setIntensity(float intensity) { mIntensity = intensity; }
		glm::vec3 getColor() const { return mColor; }
		float getIntensity() const { return mIntensity; }

	public:

		glm::vec3	mColor{ 1.0f };
		glm::vec3   mDirection{ 0.0f, -1.0f, 0.0f };
		float		mSpecularIntensity{ 1.0f };
		float		mIntensity{ 1.0 };

	};
}
