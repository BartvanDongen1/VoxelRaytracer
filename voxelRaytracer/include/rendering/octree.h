#pragma once
#include "engine\voxelModel.h"

#include <glm\vec3.hpp>
#include <vector>
#include <array>

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

//16 btyes
struct OctreeItem
{
	glm::vec3 color{ 0,0,0 };

	float padding;
};

//16 btyes
struct OctreeNode
{
	/*enum class Octant : uint8_t 
	{
		LeftTopFront =		0,
		RightTopFront =		1,
		LeftBottomFront =	2,
		RightBottomFront =	3,
		LeftTopBack =		4,
		RightTopBack =		5,
		LeftBottomBack =	6,
		RightBottomBack =	7,
	};*/

	uint32_t childrenIndex{ 0 };
	uint32_t children{ 0 }; // lowest 8 bits used as flags, 24 bits of padding

	float padding[2];
};

//16 btyes
union OctreeElement 
{
	OctreeNode node;
	OctreeItem item;
};

constexpr size_t ElementSize = sizeof(OctreeElement);

class Octree2
{
public:
	Octree2() {};
	~Octree2() {};

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