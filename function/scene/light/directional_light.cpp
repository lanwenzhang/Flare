#include "directional_light.h"


namespace flare::renderer {

	DirectionalLight::DirectionalLight(){}
	DirectionalLight::~DirectionalLight(){}

    void DirectionalLight::setDirectionFromSpherical(float theta, float phi) {
        
        const glm::mat4 rot1 = glm::rotate(glm::mat4(1.0f), glm::radians(theta), glm::vec3(0, 1, 0));
        const glm::mat4 rot2 = glm::rotate(rot1, glm::radians(phi), glm::vec3(1, 0, 0));
        const glm::vec3 lightDir = glm::normalize(glm::vec3(rot2 * glm::vec4(0.0f, -1.0f, 0.0f, 1.0f)));

        mDirection = glm::normalize(lightDir);
    }

    glm::vec3 DirectionalLight::getDirection() const {
        return mDirection;
    }

    glm::mat4 DirectionalLight::getViewMatrix() const {

        return glm::lookAt(glm::vec3(0.0f), mDirection, glm::vec3(0, 0, 1));
    }


    glm::mat4 DirectionalLight::getProjectionMatrix(float left, float right, float bottom, float top, float zNear, float zFar) const {
  
        glm::mat4 flipY = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));

        return flipY * glm::orthoLH_ZO(left, right, bottom, top, zNear, zFar);
    }
}