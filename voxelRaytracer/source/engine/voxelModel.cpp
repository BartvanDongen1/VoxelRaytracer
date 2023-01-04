#include "engine/voxelModel.h"
#include <random>

VoxelModel::VoxelModel(uint8_t aSizeX, uint8_t aSizeY, uint8_t aSizeZ) :
	sizeX(aSizeX), sizeY(aSizeY), sizeZ(aSizeZ)
{
	data = new uint32_t[aSizeX * aSizeY * aSizeZ];
}

uint32_t VoxelModel::getVoxel(uint8_t aX, uint8_t aY, uint8_t aZ) const
{
	return data[aX + aY * sizeX + aZ * sizeX * sizeY];
}

void initRandomVoxels(VoxelModel* aModel, int aFillAmount)
{
	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		bool myFilled = rand() % aFillAmount;

		myFilled = !myFilled;

		aModel->data[i] = myFilled;
	}
}
