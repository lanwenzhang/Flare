#pragma once
#include "../../../common.h"
#include "../../../application/type.h"
namespace flare::renderer {


	class Camera {
	public:

		Camera();
		~Camera();

		void setMouseControl(bool enable) { mEnableMouseControl = enable; }
		void setSpeed(float speed){ mSpeed = speed;}
		void setPosition(const glm::vec3& pos);
		void setPerspective(float angle, float ratio, float near, float far);
		void setSensitivity(float s);
		void setJitter(float x, float y);

		void lookAt(glm::vec3 pos, glm::vec3 front, glm::vec3 up);
		void update();
		void updatePitch(float deltaAngle);

		glm::vec3 getPosition() const { return mPosition; }
		glm::mat4 getViewMatrix();
		glm::mat4 getProjectMatrix();
		glm::mat4 getViewProjectionMatrix();
		glm::mat4 getJitteredProjectionMatrix() const;
		glm::mat4 getJitteredViewProjectionMatrix() const;
		
		void move(flare::app::CAMERA_MOVE mode);
		void pitch(float yOffset);
		void yaw(float xOffset);
		void onMouseMove(double xpos, double ypos);

	public:
		float       mPitchAngle;
		bool        mEnableMouseControl = false;

	private:
		glm::vec3	mPosition;
		glm::vec3	mFront;
		glm::vec3	mUp;
		float		mSpeed;

		float		mPitch;
		float		mYaw;
		float		mSensitivity;

		float		mXPos;
		float       mYPos;
		bool		mFirstMove;

		glm::mat4	mViewMatrix;
		glm::mat4	mProjectionMatrix;
		glm::mat4	mBaseProjectionMatrix;
		glm::vec2 mJitter = glm::vec2(0.0f);
		float mFov{ 0.0f };
		float mAspect{ 0.0f };
		float mNear{ 0.0f };
		float mFar{ 0.0f };
	};
}


