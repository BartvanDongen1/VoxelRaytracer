#pragma once
#include "engine\voxelModel.h"

#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

constexpr int layer1Size = 4;
constexpr int layer2Size = 4;



// grid item
// 8 bit index to atlas (0 == not filled)

// 4x4x4 voxel chunk 64 bytes
struct Layer2Chunk
{
	int items[layer1Size * layer1Size];
};

// consists of 4x4x4 chunks of chunks 
struct Layer1Chunk
{
	Layer1Chunk();

	int itemIndices[layer1Size * layer1Size * layer1Size];
};

class VoxelGrid
{
public:
	VoxelGrid() {};
	~VoxelGrid() {};

	void init(const unsigned int aSizeX, const unsigned int aSizeY, const unsigned int aSizeZ);
	void init(VoxelModel* aModel);

	void clear();

	void insertItem(const unsigned int aX, const unsigned int aY, const unsigned int aZ, int aItemIndex);

	size_t getGridSize() const;
	const void* getGridData() const;

	const void* getLayer1ChunkData() const;
	size_t getLayer1ChunkDataSize() const;

	const void* getLayer2ChunkData() const;
	size_t getLayer2ChunkDataSize() const;

	int getSizeX() const;
	int getSizeY() const;
	int getSizeZ() const;
private:

	unsigned int sizeX{ 0 };
	unsigned int sizeY{ 0 };
	unsigned int sizeZ{ 0 };

	unsigned int layer1CountX{ 0 };
	unsigned int layer1CountY{ 0 };
	unsigned int layer1CountZ{ 0 };

	int* gridLayer1Data{ nullptr }; 
	size_t gridLayer1DataSize{ 0 };

	std::vector<Layer1Chunk> layer1Chunks;
	std::vector<Layer2Chunk> layer2Chunks;
};

