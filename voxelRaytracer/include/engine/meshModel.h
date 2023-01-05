#pragma once
#include <stdint.h>
#include "aabb.h"

struct MeshModel
{
	float* positions{ nullptr };
	uint32_t vertexCount{ 0 };

	AABB aabb;
};

