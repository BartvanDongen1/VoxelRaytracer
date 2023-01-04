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
	direction = normalize(aDirection);

	aFov *= ((float)PI / 180.0f);
	float myAspectRatio = (float)Window::getWidth() / Window::getHeight();

	viewportWidth = tanf(aFov / 2) * 2;
	viewportHeight = viewportWidth / myAspectRatio;

	updateDirectionVariables();
}

glm::vec3 Camera::getUpperLeftCorner()
{
	if (dirtyDirection)
	{
		updateDirectionVariables();
	}

	return upperLeftCorner;
}

glm::vec3 Camera::getPixelOffsetHorizontal()
{
	if (dirtyDirection)
	{
		updateDirectionVariables();
	}

	return pixelOffsetHorizontal;
}

glm::vec3 Camera::getPixelOffsetVertical()
{
	if (dirtyDirection)
	{
		updateDirectionVariables();
	}

	return pixelOffsetVertical;
}

glm::vec3 Camera::getDirection()
{
	return direction;
}

void Camera::setDirection(glm::vec3 aDirection)
{
	direction = aDirection;
	dirtyDirection = true;
}

void Camera::updateDirectionVariables()
{
	// first make a vector going straight down
	glm::vec3 myDownDirection = glm::vec3(0, -1, 0);
	// do cross product with this and camera direction to get the horizontal direction
	pixelOffsetHorizontal = normalize(cross(direction, myDownDirection)) * viewportWidth;
	// do cross product with horizontal and camera direction to get the vertical
	pixelOffsetVertical = normalize(cross(direction, pixelOffsetHorizontal)) * viewportHeight;
	// get upper left corner by adding cam direction and subtracting half of vertical and horizontal
	upperLeftCorner = direction - 0.5f * pixelOffsetHorizontal - 0.5f * pixelOffsetVertical;
}
