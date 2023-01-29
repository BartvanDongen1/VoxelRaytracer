#pragma once
#include "engine\voxelModel.h"

#include <glm\vec3.hpp>
#include <vector>

class Octree
{
public:
	Octree();
	~Octree();

	void init(VoxelModel* aModel);

	int* getData() const;
private:
	void insertPoint(int aX, int aY, int aZ, uint8_t aColor);

	int* rawData;
	bool constructed{ false };
};