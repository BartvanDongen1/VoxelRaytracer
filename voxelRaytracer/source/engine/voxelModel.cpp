#include "engine/voxelModel.h"
#include <random>
#include <assert.h>
#include <algorithm>

VoxelModel::VoxelModel(int aSizeX, int aSizeY, int aSizeZ) :
	sizeX(aSizeX), sizeY(aSizeY), sizeZ(aSizeZ),
	dataSize(aSizeX * aSizeY * aSizeZ)
{
	data = new uint32_t[dataSize]{ 0 };
}

VoxelModel::~VoxelModel()
{}

void VoxelModel::combineModel(int aX, int aY, int aZ, VoxelModel * aModel)
{
	//find bounds of voxel model
	int minX = std::clamp(aX, 0, sizeX);
	int minY = std::clamp(aY, 0, sizeY);
	int minZ = std::clamp(aZ, 0, sizeZ);

	int maxX = std::clamp(aX + aModel->sizeX, 0, sizeX);
	int maxY = std::clamp(aY + aModel->sizeY, 0, sizeY);
	int maxZ = std::clamp(aZ + aModel->sizeZ, 0, sizeZ);

	/*assert(aX + aModel->sizeX <= sizeX);
	assert(aY + aModel->sizeY <= sizeY);
	assert(aZ + aModel->sizeZ <= sizeZ);*/

	for (int i = minX; i < maxX; i++)
	{
		for (int j = minY; j < maxY; j++)
		{
			for (int k = minZ; k < maxZ; k++)
			{
				// don't fill empty voxels
				if (!aModel->getVoxel(i - aX, j - aY, k - aZ)) continue;

				setVoxel(i, j, k, aModel->getVoxel(i - aX, j - aY, k - aZ));
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

void initFilled(VoxelModel* aModel, int aVoxelIndex)
{
	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		aModel->data[i] = aVoxelIndex;
	}
}

void placeFilledSphere(VoxelModel* aModel, int aX, int aY, int aZ, float aRadius, uint32_t aValue)
{
	//find bounds of sphere
	int minX = floor(std::clamp(aX - aRadius, 0.f, static_cast<float>(aModel->sizeX)));
	int minY = floor(std::clamp(aY - aRadius, 0.f, static_cast<float>(aModel->sizeY)));
	int minZ = floor(std::clamp(aZ - aRadius, 0.f, static_cast<float>(aModel->sizeZ)));

	int maxX = ceil(std::clamp(aX + aRadius, 0.f, static_cast<float>(aModel->sizeX)));
	int maxY = ceil(std::clamp(aY + aRadius, 0.f, static_cast<float>(aModel->sizeY)));
	int maxZ = ceil(std::clamp(aZ + aRadius, 0.f, static_cast<float>(aModel->sizeZ)));

	for (int x = minX; x < maxX; x++)
	{
		for (int y = minY; y < maxY; y++)
		{
			for (int z = minZ; z < maxZ; z++)
			{
				int dx = x - aX;
				int dy = y - aY;
				int dz = z - aZ;

				float dist = sqrtf(dx * dx + dy * dy + dz * dz);

				if (dist <= aRadius)
				{
					aModel->setVoxel(x, y, z, aValue);
				}
			}
		}
	}
}
