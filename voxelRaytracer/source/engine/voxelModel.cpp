#include "engine/voxelModel.h"
#include <random>
#include <assert.h>

VoxelModel::VoxelModel(int aSizeX, int aSizeY, int aSizeZ) :
	sizeX(aSizeX), sizeY(aSizeY), sizeZ(aSizeZ)
{
	data = new uint32_t[aSizeX * aSizeY * aSizeZ]{ 0 };
}

VoxelModel::~VoxelModel()
{}

void VoxelModel::combineModel(int aX, int aY, int aZ, VoxelModel * aModel)
{
	assert(aX + aModel->sizeX <= sizeX);
	assert(aY + aModel->sizeY <= sizeY);
	assert(aZ + aModel->sizeZ <= sizeZ);

	for (int i = 0; i < aModel->sizeX; i++)
	{
		for (int j = 0; j < aModel->sizeY; j++)
		{
			for (int k = 0; k < aModel->sizeZ; k++)
			{
				setVoxel(aX + i, aY + j, aZ + k, aModel->getVoxel(i, j, k));
			}
		}
	}
}

uint32_t VoxelModel::getVoxel(int aX, int aY, int aZ) const
{
	return data[aX + aY * sizeX + aZ * sizeX * sizeY];
}

void VoxelModel::setVoxel(int aX, int aY, int aZ, uint32_t aValue)
{
	assert(aX + aY * sizeX + aZ * sizeX * sizeY < sizeX * sizeY * sizeZ);

	data[aX + aY * sizeX + aZ * sizeX * sizeY] = aValue;
}

void initRandomVoxels(VoxelModel* aModel, int aVoxelIndex, int aFillAmount)
{
	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		bool myFilled = rand() % aFillAmount;

		if (!myFilled)
		{
			aModel->data[i] = aVoxelIndex;
		}
	}
}

void initFilled(VoxelModel* aModel)
{
	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		aModel->data[i] = 1;
	}
}
