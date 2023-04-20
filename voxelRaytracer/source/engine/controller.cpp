#include "engine/controller.h"
#include "engine\inputManager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

template <typename T> T clamp(T value, T low, T high)
{
	return (value < low) ? low : ((value > high) ? high : value);
}

#define PI 3.1415926535f

#define PITCH_MAX PI / 2 - 0.001f
#define PITCH_MIN -PI / 2 + 0.001f

Controller::Controller()
{
	camera = new Camera;
}

Controller::~Controller()
{
	delete camera;
}

void Controller::init()
{
	camera->init({ 1,1,1 }, { 0,0,1 }, 100.f);
}

void Controller::update(float aDeltaTime)
{
	if (!inputsEnabled) return;

	// camera rotation
	float x;
	float y;

	InputManager::getMouseDeltaPosition(&x, &y);

	yaw -= aDeltaTime * x;
	pitch -= aDeltaTime * y;

	pitch = clamp(pitch, PITCH_MIN, PITCH_MAX);

	glm::mat4 myRotateX = glm::rotate(glm::mat4(1), pitch, glm::vec3(-1, 0, 0));
	glm::mat4 myRotateY = glm::rotate(glm::mat4(1), yaw, glm::vec3(0, 1, 0));

	glm::mat4 myRotationMatrix = myRotateX * myRotateY;

	glm::vec3 mylookAtVector = glm::vec4(0, 0, 1, 0) * myRotationMatrix;
	mylookAtVector = glm::normalize(mylookAtVector);

	glm::vec3 forwardVector = glm::normalize(glm::vec3(mylookAtVector.x, 0.f, mylookAtVector.z));
	glm::vec3 upVector = glm::vec3(0, 1, 0);
	glm::vec3 rightVector = glm::cross(upVector, forwardVector);


	glm::vec3 tempAcceleration = glm::vec3(0.f);

	// speed scale
	speedScale += 0.1f * InputManager::getMouseScroll();

	if (speedScale < minSpeedScale) speedScale = minSpeedScale;
	if (speedScale > maxSpeedScale) speedScale = maxSpeedScale;

	// camera movement
	if (InputManager::getKey(Keys::w)->heldDown)
	{
		tempAcceleration += forwardVector * acceleration * speedScale * aDeltaTime;
	}

	if (InputManager::getKey(Keys::s)->heldDown)
	{
		tempAcceleration -= forwardVector * acceleration * speedScale * aDeltaTime;
	}

	if (InputManager::getKey(Keys::a)->heldDown)
	{
		tempAcceleration -= rightVector * acceleration * speedScale * aDeltaTime;
	}

	if (InputManager::getKey(Keys::d)->heldDown)
	{
		tempAcceleration += rightVector * acceleration * speedScale * aDeltaTime;
	}

	if (InputManager::getKey(Keys::q)->heldDown)
	{
		tempAcceleration += upVector * acceleration * speedScale * aDeltaTime;
	}

	if (InputManager::getKey(Keys::e)->heldDown)
	{
		tempAcceleration -= upVector * acceleration * speedScale * aDeltaTime;
	}

	// handle deceleration
	tempAcceleration -= deceleration * velocity * aDeltaTime;

	velocity += tempAcceleration;

	// manage max velocity
	if (velocity.x != 0 || velocity.y != 0 || velocity.z != 0)
	{
		float velocityForce = glm::length(velocity);

		if (velocityForce > maxVelocity * speedScale)
		{
			velocity = normalize(velocity) * maxVelocity * speedScale;
		}

		position += velocity * aDeltaTime;
	}

	camera->setDirection(mylookAtVector);
	camera->position = position;
}

void Controller::enableInputs()
{
	inputsEnabled = true;
}

void Controller::disableInputs()
{
	inputsEnabled = false;
}

bool Controller::getInputsEnabled() const
{
	return inputsEnabled;
}
