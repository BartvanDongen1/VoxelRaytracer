#include "rendering/camera.h"
#include "window.h"

#define PI 3.1415926535f

Camera::Camera()
{}

Camera::~Camera()
{}

void Camera::init(glm::vec3 aPosition, glm::vec3 aDirection, float aFov)
{
	position = aPosition;
	direction = aDirection;

	aFov *= ((float)PI / 180.0f);
	float myAspectRatio = (float)Window::getWidth() / Window::getHeight();

	viewportWidth = tanf(aFov / 2) * 2;
	viewportHeight = viewportWidth / myAspectRatio;

	// first make a vector going straight down
	glm::vec3 myDownDirection = glm::vec3(0, -1, 0);
	// do cross product with this and camera direction to get the horizontal direction
	pixelOffsetHorizontal = normalize(cross(direction, myDownDirection)) * viewportWidth;
	// do cross product with horizontal and camera direction to get the vertical
	pixelOffsetVertical = normalize(cross(direction, pixelOffsetHorizontal)) * viewportHeight;
	// get upper left corner by adding cam direction and subtracting half of vertical and horizontal
	upperLeftCorner = direction - 0.5f * pixelOffsetHorizontal - 0.5f * pixelOffsetVertical;
}

glm::vec3 Camera::getPosition() const
{
	return position;
}

glm::vec3 Camera::getDirection() const
{
	return direction;
}

glm::vec3 Camera::getUpperLeftCorner() const
{
	return upperLeftCorner;
}

glm::vec3 Camera::getPixelOffsetHorizontal() const
{
	return pixelOffsetHorizontal;
}

glm::vec3 Camera::getPixelOffsetVertical() const
{
	return pixelOffsetVertical;
}
