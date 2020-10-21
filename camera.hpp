#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>


class Camera
{
public:
	glm::vec3 cameraPos;
	glm::vec3 cameraDir;
	glm::vec3 cameraUp;

	glm::vec2 rotation = glm::vec2();

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;

	bool updated = false;

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
		return keys.left || keys.right || keys.up || keys.down;
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
		cameraDir = glm::normalize(center - cameraPos);
		cameraUp = glm::normalize(up);
		matrices.view = glm::lookAt(eye, center, up);
	}

	void rotate(const glm::vec2& delta)
	{
		rotation += delta;

		if (rotation.y > 89.0f)
			rotation.y = 89.0f;
		else if (rotation.y < -89.0f)
			rotation.y = -89.0f;
	}

	void update(float deltaTime)
	{
		updated = false;

		glm::vec3 dir;
		dir.x = -cos(glm::radians(rotation.y)) * sin(glm::radians(rotation.x));
		dir.y = sin(glm::radians(rotation.y));
		dir.z = cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
		cameraDir = glm::normalize(dir);

		float moveSpeed = deltaTime * movementSpeed;

		if (keys.up)
			cameraPos += cameraDir * moveSpeed;
		if (keys.down)
			cameraPos -= cameraDir * moveSpeed;
		if (keys.left)
			cameraPos -= glm::normalize(glm::cross(cameraDir, cameraUp)) * moveSpeed;
		if (keys.right)
			cameraPos += glm::normalize(glm::cross(cameraDir, cameraUp)) * moveSpeed;

		matrices.view = glm::lookAt(cameraPos, cameraPos + cameraDir, cameraUp);
		updated = true;
	}

private:
	float fov;
	float znear, zfar;
};