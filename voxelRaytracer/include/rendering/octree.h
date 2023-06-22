#pragma once
#include "engine\voxelModel.h"

#include <glm\vec3.hpp>
#include <vector>
#include <array>

//16 btyes
struct OctreeItem
{
	float padding[3];

	int data;
};

//16 btyes
struct OctreeNode
{
	uint32_t childrenIndex{ 0 };
	uint32_t children{ 0 }; // lowest 8 bits used as flags, 24 bits of padding
	uint32_t parentIndex{ 0 };
	uint32_t parentOctant{ 0 };
};

//16 btyes
union OctreeElement 
{
	OctreeNode node;
	OctreeItem item;
};

constexpr size_t ElementSize = sizeof(OctreeElement);

class Octree
{
public:
	Octree() {};
	~Octree() {};

	void init(VoxelModel* aModel);
	void init(int aSizeX, int aSizeY, int aSizeZ);
	void insertItem(int aX, int aY, int aZ, OctreeItem aItem);

	const void* getData() const;
	size_t getSize() const;
	int getLayerCount() const;
private:
	std::vector<std::array<OctreeElement, 8>> flatTree;
	
	int size = 0;
	int layerCount = 0;
};