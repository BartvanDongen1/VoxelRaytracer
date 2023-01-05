#pragma once
#include <stdint.h>

struct VoxelModel
{
	VoxelModel() {};
	VoxelModel(int aSizeX, int aSizeY, int aSizeZ);

	~VoxelModel();

	void combineModel(int aX, int aY, int aZ, VoxelModel* aModel);

	uint32_t getVoxel(int aX, int aY, int aZ) const;
	void setVoxel(int aX, int aY, int aZ, uint32_t aValue);

	const int sizeX{ 0 };
	const int sizeY{ 0 };
	const int sizeZ{ 0 };

	uint32_t* data{ nullptr };
};

void initRandomVoxels(VoxelModel* aModel, int aFillAmount = 10);