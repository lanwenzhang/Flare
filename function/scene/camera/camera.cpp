#include "camera.h"

namespace flare::renderer {

	Camera::Camera()
	{
		mPosition = glm::vec3(1.0f);
		mFront = glm::vec3(1.0f);
		mUp = glm::vec3(1.0f);
		mSpeed = 0.01f;

		mPitch = 0.0f;
		mYaw = -90.0f;
		mSensitivity = 0.1f;

		mXPos = 0;
		mYPos = 0;
		mFirstMove = true;

		mViewMatrix = glm::mat4(1.0f);
	}

	Camera::~Camera() {}

	void Camera::lookAt(glm::vec3 pos, glm::vec3 front, glm::vec3 up){
		mPosition = pos;
		mFront = front;
		mUp = up;

		mViewMatrix = glm::lookAt(mPosition, mPosition + mFront, mUp);
	}

	void Camera::update(){
		mViewMatrix = glm::lookAt(mPosition, mPosition + mFront, mUp);
	}

	void Camera::updatePitch(float deltaAngle){

		glm::vec3 right = glm::normalize(glm::cross(mFront, mUp));

		glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(deltaAngle),right);

		glm::vec4 newFront = rot * glm::vec4(mFront, 0.0f);
		glm::vec4 newUp = rot * glm::vec4(mUp, 0.0f);

		mFront = glm::normalize(glm::vec3(newFront));
		mUp = glm::normalize(glm::vec3(newUp));
		update();
	}

	glm::mat4 Camera::getViewMatrix()
	{
		return mViewMatrix;
	}

	glm::mat4 Camera::getProjectMatrix() {

		return mProjectionMatrix;
	}

	glm::mat4 Camera::getViewProjectionMatrix() {

		return mProjectionMatrix * mViewMatrix;

	}

	void Camera::move(flare::app::CAMERA_MOVE mode){
		switch (mode)
		{
		case flare::app::CAMERA_MOVE::MOVE_LEFT:
			mPosition -= glm::normalize(glm::cross(mFront, mUp)) * mSpeed;
			break;
		case flare::app::CAMERA_MOVE::MOVE_RIGHT:
			mPosition += glm::normalize(
				glm::cross(mFront, mUp)) * mSpeed;
			break;
		case flare::app::CAMERA_MOVE::MOVE_FRONT:
			mPosition += mSpeed * mFront;
			break;
		case flare::app::CAMERA_MOVE::MOVE_BACK:
			mPosition -= mSpeed * mFront;
			break;
		default:
			break;
		}

		update();
	}

	void Camera::pitch(float yOffset)
	{
		mPitch += yOffset * mSensitivity;

		if (mPitch >= 89.0f)
		{
			mPitch = 89.0f;
		}

		if (mPitch <= -89.0f)
		{
			mPitch = -89.0f;
		}

		mFront.y = sin(glm::radians(mPitch));
		mFront.x = cos(glm::radians(mYaw)) * cos(glm::radians(mPitch));
		mFront.z = sin(glm::radians(mYaw)) * cos(glm::radians(mPitch));

		mFront = glm::normalize(mFront);
		update();
	}

	void Camera::yaw(float xOffset)
	{
		mYaw += xOffset * mSensitivity;

		mFront.y = sin(glm::radians(mPitch));
		mFront.x = cos(glm::radians(mYaw)) * cos(glm::radians(mPitch));
		mFront.z = sin(glm::radians(mYaw)) * cos(glm::radians(mPitch));

		mFront = glm::normalize(mFront);
		update();
	}

	void Camera::setSensitivity(float s)
	{
		mSensitivity = s;
	}

	void Camera::setPerspective(float angle, float ratio, float near, float far) {
		mFov = angle; mAspect = ratio; mNear = near; mFar = far;
		mBaseProjectionMatrix = glm::perspective(glm::radians(angle), ratio, near, far);
		mBaseProjectionMatrix[1][1] *= -1.0f;
		mProjectionMatrix = mBaseProjectionMatrix;
	}

	void Camera::setPosition(const glm::vec3& pos)
	{
		mPosition = pos;
		update();
	}

	void Camera::setJitter(float x, float y) {
		mJitter = glm::vec2(x, y);
	}

	glm::mat4 Camera::getJitteredProjectionMatrix() const {
		glm::mat4 jitterMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(mJitter.x, mJitter.y, 0.0f));
		return jitterMatrix * mProjectionMatrix;
	}

	glm::mat4 Camera::getJitteredViewProjectionMatrix() const {
		glm::mat4 jitterMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(mJitter.x, mJitter.y, 0.0f));
		return jitterMatrix * mProjectionMatrix * mViewMatrix;
	}

	void Camera::onMouseMove(double xpos, double ypos)
	{

		if (mFirstMove)
		{
			mXPos = xpos;
			mYPos = ypos;
			mFirstMove = false;
			return;
		}

		float _xOffset = xpos - mXPos;
		float _yOffset = -(ypos - mYPos);

		mXPos = xpos;
		mYPos = ypos;

		if (mEnableMouseControl) {
			pitch(_yOffset);
			yaw(_xOffset);
		}
	}
}

