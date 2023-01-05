#pragma once
#include <glm/glm.hpp>

#undef min
#undef max

#define MAX(a,b) (((a)>(b))?(a):(b))

struct AABB
{
	glm::vec3 min;
	glm::vec3 max;

	void initInvertedInfinity()
	{
		min = glm::vec3(FLT_MAX);
		max = glm::vec3(-FLT_MAX);
	}

	void growToContain(glm::vec3 aPosition)
	{
		min = glm::min(min, aPosition);
		max = glm::max(max, aPosition);
	}

	glm::vec3 getDimensions() const
	{
		glm::vec3 result = max - min;
		return result;
	}

	glm::vec3 getCenter() const
	{
		glm::vec3 result = 0.5f * (min + max);
		return result;
	}

	glm::vec3 calculateNormalizedPosition(glm::vec3 aPosition) const
	{
		glm::vec3 centeredPosition = aPosition - min;
		glm::vec3 dimensions = getDimensions();

		float max = MAX(MAX(dimensions.x, dimensions.y), dimensions.z);
		glm::vec3 squaredDimention{ max , max , max };


		return centeredPosition / squaredDimention;
	}
};
