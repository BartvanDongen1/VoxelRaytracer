#pragma once
#include <glm/glm.hpp>
#include "rendering/camera.h"

class BenchmarkManager;

class Controller
{
	friend class BenchmarkManager;
public:
	Controller();
	~Controller();

	void init();
	void update(float aDeltaTime);

	Camera* getCamera() const { return camera; };

	float mouseSensitivityMultiplier{ 1 };

	float acceleration{ 20 };
	float deceleration{ 5 };

	float maxVelocity{ 4 };

	void enableInputs();
	void disableInputs();

	bool getInputsEnabled() const;
private:
	bool inputsEnabled{ true };

	Camera* camera;

	glm::vec3 position{ 0, 0, 0 };
	glm::vec3 velocity{ 0, 0, 0 };

	float pitch{ 0 };
	float yaw{ 0 };

	float speedScale{ 1.f };
	float minSpeedScale{ 0.1f };
	float maxSpeedScale{ 5.f };
};

