#include "rendering\octree.h"
#include <glm\glm.hpp>
#include "engine\logger.h"

constexpr size_t octreeSize = 
1 +
8 +
8 * 8 +
8 * 8 * 8 +
8 * 8 * 8 * 8;

Octree::Octree(glm::uvec3 aMin, glm::uvec3 aMax, Octree* aParent)
{
	min = aMin;
	max = aMax;

	parent = aParent;

	if ((max - min).x > 1)
	{
		glm::uvec3 mid = (min + max) / glm::uvec3(2);

		children[0] = new Octree({ min.x, min.y, min.z }, { mid.x, mid.y, mid.z }, this);
		children[1] = new Octree({ mid.x, min.y, min.z }, { max.x, mid.y, mid.z }, this);
		children[2] = new Octree({ min.x, mid.y, min.z }, { mid.x, max.y, mid.z }, this);
		children[3] = new Octree({ mid.x, mid.y, min.z }, { max.x, max.y, mid.z }, this);
		children[4] = new Octree({ min.x, min.y, mid.z }, { mid.x, mid.y, max.z }, this);
		children[5] = new Octree({ mid.x, min.y, mid.z }, { max.x, mid.y, max.z }, this);
		children[6] = new Octree({ min.x, mid.y, mid.z }, { mid.x, max.y, max.z }, this);
		children[7] = new Octree({ mid.x, mid.y, mid.z }, { max.x, max.y, max.z }, this);
	}
}

Octree::~Octree()
{
	if (constructed)
	{
		delete rawData;
	}
}

void Octree::init(VoxelModel* aModel)
{
	min = { 0,0,0 };
	max = { aModel->sizeX, aModel->sizeY, aModel->sizeZ };

	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		uint32_t myPointData = aModel->data[i];

		if (myPointData)
		{
			int myX = i % 32;
			int myY = (i / 32) % 32;
			int myZ = i / (32 * 32);

			insertPoint(myX, myY, myZ, 1);
		}
	}
}

void Octree::constructData()
{
	if (constructed)
	{
		LOG_WARNING("Octree already constructed");
	}

	rawData = new int[octreeSize] { 0 };


}

void Octree::insertPoint(int aX, int aY, int aZ, uint8_t aColor)
{
	glm::uvec3 myPoint{ aX, aY, aZ };

	empty = false;

	if ((max - min) == glm::uvec3(1,1,1))
	{
		// at leaf node
		color = aColor;
		return;
	}

	glm::uvec3 mid = (min + max) / glm::uvec3(2);

	glm::uvec3 octant{ 0,0,0 };


	if (myPoint.x > mid.x) octant.x = 1;
	if (myPoint.y > mid.y) octant.y = 1;
	if (myPoint.z > mid.z) octant.z = 1;

	int childIndex = octant.x + octant.y * 2 + octant.z * 4;

	children[childIndex]->insertPoint(aX, aY, aZ, aColor);
}
