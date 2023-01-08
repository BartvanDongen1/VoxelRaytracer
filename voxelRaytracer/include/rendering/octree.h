#pragma once
#include "engine\voxelModel.h"

#include <glm\vec3.hpp>
#include <vector>

class Octree
{
public:
	Octree(glm::uvec3 aMin, glm::uvec3 aMax, Octree* aParent = nullptr);
	~Octree();

	void init(VoxelModel* aModel);
	void constructData();

private:
	void insertPoint(int aX, int aY, int aZ, uint8_t aColor);

	int* rawData;
	bool constructed{ false };

	bool empty{ true };
	uint8_t color;
	
	glm::uvec3 min;
	glm::uvec3 max;

	Octree* parent;
	Octree* children[8];
};