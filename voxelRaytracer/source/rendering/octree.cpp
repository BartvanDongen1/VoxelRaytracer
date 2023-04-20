#include "rendering\octree.h"
#include <glm\glm.hpp>
#include "engine\logger.h"

void Octree::init(VoxelModel* aModel)
{
	init(aModel->sizeX, aModel->sizeY, aModel->sizeZ);

	int count = 0;
	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		uint32_t myPointData = aModel->data[i];

		if (myPointData)
		{
			count++;

			int myX = i % aModel->sizeX;
			int myY = (i / aModel->sizeX) % aModel->sizeY;
			int myZ = i / (aModel->sizeX * aModel->sizeY);

			assert(myX < aModel->sizeX && myY < aModel->sizeY && myZ < aModel->sizeZ);

			OctreeItem myItem;
			myItem.color = glm::vec3(1, 1, 1);

			insertItem(myX, myY, myZ, myItem);
		}
	}
	LOG_INFO("voxels in octree: %i", count);
}

void Octree::init(int aSizeX, int aSizeY, int aSizeZ)
{
	int myMaxSize = std::max(std::max(aSizeX, aSizeY), aSizeZ);

	assert(myMaxSize > 1);

	layerCount = 1;
	int myNextMultiple = 1;
	while (myNextMultiple < myMaxSize) {
		myNextMultiple *= 2;
		layerCount++;
	}

	size = myNextMultiple;
}

void Octree::insertItem(int aX, int aY, int aZ, OctreeItem aItem)
{
	assert(aX < size&& aX >= 0);
	assert(aY < size&& aY >= 0);
	assert(aZ < size&& aZ >= 0);

	// push top level node and first sub nodes
	if (!flatTree.size())
	{
		flatTree.push_back({});
		flatTree.push_back({});
	}

	// top level node will be the index 0 in the array that is index 0 in the vector
	OctreeNode* myTopLevelNode = &flatTree[0][0].node;
	myTopLevelNode->childrenIndex = 1; // child index of the top level node will always be 1

	int myScale = size;
	uint32_t myNodeIndex = 1;
	
	int myLocalX = aX, myLocalY = aY, myLocalZ = aZ;
	OctreeNode* myCurrentNode = myTopLevelNode;
	while (true)
	{
		int octantX = (myLocalX >= (myScale / 2.f));
		int octantY = (myLocalY >= (myScale / 2.f));
		int octantZ = (myLocalZ >= (myScale / 2.f));

		myLocalX -= octantX * (myScale / 2);
		myLocalY -= octantY * (myScale / 2);
		myLocalZ -= octantZ * (myScale / 2);

		int myOctantOffset = octantX + octantY * 2 + octantZ * 4;

		if (myCurrentNode)
		{
			int test = (1 << myOctantOffset);
			myCurrentNode->children |= (1 << myOctantOffset);
		}

		if (myScale == 2)
		{
			//place leaf node
			flatTree[myNodeIndex][myOctantOffset].item = aItem;
			break;
		}

		myCurrentNode = &flatTree[myNodeIndex][myOctantOffset].node;

		if (myCurrentNode->childrenIndex == 0)
		{
			//set new children index
			myCurrentNode->childrenIndex = static_cast<uint32_t>(flatTree.size());

			// create new nodes
			flatTree.push_back({});

			// reassign the current node, because reallocation of memory could've moved it
			myCurrentNode = &flatTree[myNodeIndex][myOctantOffset].node;
		}

		myNodeIndex = myCurrentNode->childrenIndex;
		myScale /= 2;
	}
}

const void* Octree::getData() const
{
	assert(flatTree.size() > 0); //can't use empty octree

	return &flatTree[0];
}

size_t Octree::getSize() const
{
	return flatTree.size();
}

int Octree::getLayerCount() const
{
	return layerCount;
}
