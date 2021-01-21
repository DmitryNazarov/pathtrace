#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


class Camera
{
public:
	glm::vec3 cameraPos;
	glm::vec3 cameraDir;
	glm::vec3 cameraCenter;
	glm::vec3 cameraUp;
	glm::vec2 rotation = glm::vec2();

	float rotationSpeed = 1.0f;
	float movementSpeed = 5.0f;

	bool updated = false;
	bool moved = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool moving()
	{
		return keys.left || keys.right || keys.up || keys.down || moved;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	};

	void updateAspectRatio(float aspect)
	{
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
	}

	void setLookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up)
	{
		cameraPos = eye;
		cameraCenter = center;
		cameraDir = glm::normalize(center - cameraPos);
		cameraUp = glm::normalize(up);
		moved = true;
	}

	void rotate(const glm::vec2& delta)
	{
		changeHeading(delta.x);
		changePitch(delta.y);
		moved = true;
	}

	void update(float deltaTime)
	{
		updated = false;

		if (!moving())
		{
			return;
		}

		//update camera orientation
		cameraDir = glm::normalize(cameraCenter - cameraPos);
		//determine axis for pitch rotation
		glm::vec3 axis = glm::cross(cameraDir, cameraUp);
		//compute quaternion for pitch based on the camera pitch angle
		glm::quat pitch_quat = glm::angleAxis(glm::radians(rotation.y), axis);
		//determine heading quaternion from the camera up vector and the heading angle
		glm::quat heading_quat = glm::angleAxis(glm::radians(rotation.x), cameraUp);
		//add the two quaternions
		glm::quat temp = glm::normalize(glm::cross(pitch_quat, heading_quat));
		//update the direction from the quaternion
		cameraDir = glm::rotate(temp, cameraDir);

		//update camera position
		float moveSpeed = deltaTime * movementSpeed;
		if (keys.up)
			cameraPos += cameraDir * moveSpeed;
		if (keys.down)
			cameraPos -= cameraDir * moveSpeed;
		if (keys.left)
			cameraPos -= glm::normalize(glm::cross(cameraDir, cameraUp)) * moveSpeed;
		if (keys.right)
			cameraPos += glm::normalize(glm::cross(cameraDir, cameraUp)) * moveSpeed;

		//set the look at to be in front of the camera
		cameraCenter = cameraPos + cameraDir;

		matrices.view = glm::lookAt(cameraPos, cameraCenter, cameraUp);

		rotation = glm::vec2();
		moved = false;
		updated = true;
	}

private:
	void changeHeading(float degrees)
	{
		//Check bounds with the max heading rate so that we aren't moving too fast
		checkMaxRateBounds(degrees, maxHeadingRate);

		//This controls how the heading is changed if the camera is pointed straight up or down
		//The heading delta direction changes
		if ((rotation.y > 90 && rotation.y < 270) || (rotation.y < -90 && rotation.y > -270))
		{
			rotation.x -= degrees;
		}
		else
		{
			rotation.x += degrees;
		}

		//Check bounds for the camera heading
		checkAngleBounds(rotation.x);
	}

	void changePitch(float degrees)
	{
		//Check bounds with the max pitch rate so that we aren't moving too fast
		checkMaxRateBounds(degrees, maxPitchRate);

		rotation.y += degrees;

		//Check bounds for the camera pitch
		checkAngleBounds(rotation.y);
	}

	void checkMaxRateBounds(float& angle, float maxRate)
	{
		if (angle < -maxRate)
		{
			angle = -maxRate;
		}
		else if (angle > maxRate)
		{
			angle = maxRate;
		}
	}

	void checkAngleBounds(float& angle)
	{
		if (angle > 360.0f)
		{
			angle -= 360.0f;
		}
		else if (angle < -360.0f)
		{
			angle += 360.0f;
		}
	}

private:
	float fov;
	float znear, zfar;

	float maxPitchRate = 5.0f;
	float maxHeadingRate = 5.0f;
};
