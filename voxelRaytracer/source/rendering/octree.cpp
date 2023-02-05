#include "rendering\octree.h"
#include <glm\glm.hpp>
#include "engine\logger.h"

constexpr size_t layer0offset =
0;

constexpr size_t layer1offset =
1;

constexpr size_t layer2offset =
layer1offset +
8;

constexpr size_t layer3offset =
layer2offset +
8 * 8;

constexpr size_t layer4offset =
layer3offset +
8 * 8 * 8;

constexpr size_t octreeSize = 
layer4offset +
8 * 8 * 8 * 8;

constexpr size_t myParentOffsetOffset = 0;

constexpr size_t myChildOffsetOffset = 10;

constexpr size_t myFilledOffset = 23;

constexpr size_t myColorOffset = 24;

//octree node data -> 32 bits

// 24 bits octree traveral & 8 bits color data
	// 10 bits parent offset data -> offset 0
	// 13 bits children offset data -> offset 10
	// 1 bit filled in bool -> offset 23
	// 8 bits voxel data -> offset 24

Octree::Octree()
{}

Octree::~Octree()
{
	if (constructed)
	{
		delete rawData;
	}
}

void Octree::init(VoxelModel* aModel)
{
	rawData = new int[octreeSize] { 0 };
	constructed = true;

	for (int i = 0; i < aModel->sizeX * aModel->sizeY * aModel->sizeZ; i++)
	{
		uint32_t myPointData = aModel->data[i];

		if (myPointData)
		{
			int myX = i % 16;
			int myY = (i / 16) % 16;
			int myZ = i / (16 * 16);

			assert(myX < 16 && myY < 16 && myZ < 16);

			insertPoint(myX, myY, myZ, static_cast<uint8_t>(myPointData));
		}
	}
}

int* Octree::getData() const
{
	return rawData;
}

void Octree::insertPoint(int aX, int aY, int aZ, uint8_t aColor)
{
	//0th layer insertion
	{
		int myData = 0;

		myData |= layer1offset << myChildOffsetOffset;
		
		//layer 0 has no parent
		
		myData |= 0b1 << myFilledOffset;

		rawData[0] = myData;
	}

	int myChildLayersOffset = 0;

	int myLayer1Offset;
	//1st layer instertion
	{
		int myData = 0;

		int myXOctant = aX >> 3;
		int myYOctant = aY >> 3;
		int myZOctant = aZ >> 3;

		int layerOffset = static_cast<int>(myXOctant) + static_cast<int>(myYOctant) * 2 + static_cast<int>(myZOctant) * 4;
		myLayer1Offset = layer1offset + layerOffset;

		myChildLayersOffset = layerOffset << 3;

		myData |= layer2offset + myChildLayersOffset << myChildOffsetOffset;
		
		myData |= layer0offset << myParentOffsetOffset;
		
		myData |= 0b1 << myFilledOffset;

		rawData[myLayer1Offset] = myData;
	}

	int myLayer2Offset;
	//2nd layer insertion
	{
		int myData = 0;

		int myXOctant = (aX & 0b100) >> 2;
		int myYOctant = (aY & 0b100) >> 2;
		int myZOctant = (aZ & 0b100) >> 2;

		int layerOffset = static_cast<int>(myXOctant) + static_cast<int>(myYOctant) * 2 + static_cast<int>(myZOctant) * 4;
		myLayer2Offset = layer2offset + myChildLayersOffset + layerOffset;

		myChildLayersOffset <<= 3;
		myChildLayersOffset += layerOffset << 3;

		myData |= layer3offset + myChildLayersOffset << myChildOffsetOffset;

		myData |= myLayer1Offset << myParentOffsetOffset;

		myData |= 0b1 << myFilledOffset;

		rawData[myLayer2Offset] = myData;
	}

	int myLayer3Offset;
	//3rd layer insertion
	{
		int myData = 0;

		int myXOctant = (aX & 0b10) >> 1;
		int myYOctant = (aY & 0b10) >> 1;
		int myZOctant = (aZ & 0b10) >> 1;

		int layerOffset = static_cast<int>(myXOctant) + static_cast<int>(myYOctant) * 2 + static_cast<int>(myZOctant) * 4;
		myLayer3Offset = layer3offset + myChildLayersOffset + layerOffset;

		myChildLayersOffset <<= 3;
		myChildLayersOffset += layerOffset << 3;

		myData |= layer4offset + myChildLayersOffset << myChildOffsetOffset;

		myData |= myLayer2Offset << myParentOffsetOffset;

		myData |= 0b1 << myFilledOffset;

		rawData[myLayer3Offset] = myData;
	}

	int myLayer4Offset;
	//4th layer insertion
	{
		int myData = 0;

		int myXOctant = (aX & 0b1);
		int myYOctant = (aY & 0b1);
		int myZOctant = (aZ & 0b1);

		int layerOffset = static_cast<int>(myXOctant) + static_cast<int>(myYOctant) * 2 + static_cast<int>(myZOctant) * 4;
		myLayer4Offset = layer4offset + myChildLayersOffset + layerOffset;

		//leaf node has no child

		myData |= myLayer3Offset << myParentOffsetOffset;

		myData |= 0b1 << myFilledOffset;

		myData |= aColor << myColorOffset;

		rawData[myLayer4Offset] = myData;
	}
}
