#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
	Camera();
	~Camera();

	void init(glm::vec3 aPosition, glm::vec3 aDirection, float aFov);
	
	glm::vec3 getPosition() const;
	glm::vec3 getDirection() const;
	glm::vec3 getUpperLeftCorner() const;
	glm::vec3 getPixelOffsetHorizontal() const;
	glm::vec3 getPixelOffsetVertical() const;
private:
	glm::vec3 position{ 0,0,0 };
	glm::vec3 direction{ 0,0,0 };

	glm::vec3 upperLeftCorner{ 0,0,0 };
	glm::vec3 pixelOffsetHorizontal{ 0,0,0 };
	glm::vec3 pixelOffsetVertical{ 0,0,0 };

	float viewportWidth{ 0.f };
	float viewportHeight{ 0.f };
};

