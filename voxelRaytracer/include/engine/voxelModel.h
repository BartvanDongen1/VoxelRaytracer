#pragma once
#include <stdint.h>

struct VoxelModel
{
	VoxelModel(uint8_t aSizeX, uint8_t aSizeY, uint8_t aSizeZ);

	uint32_t getVoxel(uint8_t aX, uint8_t aY, uint8_t aZ) const;

	const uint8_t sizeX;
	const uint8_t sizeY;
	const uint8_t sizeZ;

	uint32_t* data;
};

void initRandomVoxels(VoxelModel* aModel, int aFillAmount = 10);