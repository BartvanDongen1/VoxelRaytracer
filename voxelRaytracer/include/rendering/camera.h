#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
	Camera();
	~Camera();

	void init(glm::vec3 aPosition = glm::vec3(1,1,1), glm::vec3 aDirection = glm::vec3(0, 0, 1), float aFov = 90.0);
	
	glm::vec3 getUpperLeftCorner();
	glm::vec3 getPixelOffsetHorizontal();
	glm::vec3 getPixelOffsetVertical();

	glm::vec3 getDirection() const;
	void setDirection(glm::vec3 aDirection);

	glm::vec3 position{ 0,0,0 };

private:
	void updateDirectionVariables();
	
	bool controlsEnabled;

	glm::vec3 direction{ 0,0,0 };

	glm::vec3 upperLeftCorner{ 0,0,0 };
	glm::vec3 pixelOffsetHorizontal{ 0,0,0 };
	glm::vec3 pixelOffsetVertical{ 0,0,0 };

	float viewportWidth{ 0.f };
	float viewportHeight{ 0.f };

	bool dirtyDirection{ false };
};

