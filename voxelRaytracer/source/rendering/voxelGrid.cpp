#include "rendering/voxelGrid.h"
#include "engine/logger.h"

void VoxelGrid::init(const unsigned int aSizeX, const unsigned int aSizeY, const unsigned int aSizeZ)
{
	sizeX = aSizeX;
	sizeY = aSizeY;
	sizeZ = aSizeZ;

	layer1CountX = sizeX / (layer1Size * layer2Size);
	layer1CountY = sizeY / (layer1Size * layer2Size);
	layer1CountZ = sizeZ / (layer1Size * layer2Size);

	gridLayer1DataSize = layer1CountX * layer1CountY * layer1CountZ;
	gridLayer1Data = new int[gridLayer1DataSize];

	clear();
}

void VoxelGrid::init(VoxelModel* aModel)
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

			assert(myX < aModel->sizeX&& myY < aModel->sizeY&& myZ < aModel->sizeZ);

			GridItem myItem;
			myItem.color = glm::vec3(1, 1, 1);

			insertItem(myX, myY, myZ, myItem);
		}
	}

	LOG_INFO("chunks in voxel grid: %i", layer2Chunks.size());
	LOG_INFO("voxels in voxel grid: %i", count);
}

void VoxelGrid::clear()
{
	for (int i = 0; i < gridLayer1DataSize; i++)
	{
		gridLayer1Data[i] = -1;
	}

	layer1Chunks.clear();
	layer2Chunks.clear();
}

void VoxelGrid::insertItem(const unsigned int aX, const unsigned int aY, const unsigned int aZ, GridItem aItem)
{
	// get layer 1 xyz and index
	const uint32_t myLayer1ChunkX = aX / (layer1Size * layer2Size);
	const uint32_t myLayer1ChunkY = aY / (layer1Size * layer2Size);
	const uint32_t myLayer1ChunkZ = aZ / (layer1Size * layer2Size);
	
	const uint32_t myLayer1ChunkIndex = myLayer1ChunkX + (myLayer1ChunkY * layer1CountX) + (myLayer1ChunkZ * layer1CountX * layer1CountY);
	assert(myLayer1ChunkIndex < gridLayer1DataSize);

	// add chunk to the vector if it doesn't exist yet
	if (gridLayer1Data[myLayer1ChunkIndex] == -1)
	{
		gridLayer1Data[myLayer1ChunkIndex] = layer1Chunks.size();
		layer1Chunks.push_back({});
	}

	// get chunk
	Layer1Chunk& myLayer1chunk = layer1Chunks[gridLayer1Data[myLayer1ChunkIndex]];
	
	// get layer 2 xyz
	const uint32_t myLayer2ChunkX = (aX - myLayer1ChunkX * (layer1Size * layer2Size)) / layer1Size;
	const uint32_t myLayer2ChunkY = (aY - myLayer1ChunkY * (layer1Size * layer2Size)) / layer1Size;
	const uint32_t myLayer2ChunkZ = (aZ - myLayer1ChunkZ * (layer1Size * layer2Size)) / layer1Size;
	
	const uint32_t myLayer2ChunkIndex = myLayer2ChunkX + (myLayer2ChunkY * layer1Size) + (myLayer2ChunkZ * layer1Size * layer1Size);
	
	// add chunk to the vector if it doesn't exist yet
	if (myLayer1chunk.itemIndices[myLayer2ChunkIndex] == -1)
	{
		myLayer1chunk.itemIndices[myLayer2ChunkIndex] = layer2Chunks.size();
		layer2Chunks.push_back({});
	}

	// get chunk
	Layer2Chunk& mychunk = layer2Chunks[myLayer1chunk.itemIndices[myLayer2ChunkIndex]];
	
	// add item to chunk
	const uint32_t myX = (aX % (layer1Size * layer2Size)) % layer1Size;
	const uint32_t myY = (aY % (layer1Size * layer2Size)) % layer1Size;
	const uint32_t myZ = (aZ % (layer1Size * layer2Size)) % layer1Size;

	const uint32_t myItemIndex = myX + (myY * layer2Size) + (myZ * layer2Size * layer2Size);

	assert(myItemIndex < layer2Size* layer2Size* layer2Size);

	aItem.filled = 1.f;
	mychunk.items[myItemIndex] = aItem;

	/*const uint32_t myChunkIndex = myChunkX + (myChunkY * layer1CountX) + (myChunkZ * layer1CountX * layer1CountY);

	assert(myChunkIndex < gridDataSize);

	if (gridLayer1Data[myChunkIndex] == -1)
	{
		gridLayer1Data[myChunkIndex] = layer2Chunks.size();
		layer2Chunks.push_back({});
	}

	VoxelChunk& mychunk = layer2Chunks[gridData[myChunkIndex]];

	const uint32_t myX = aX % layer1Size;
	const uint32_t myY = aY % layer1Size;
	const uint32_t myZ = aZ % layer1Size;

	const uint32_t myItemIndex = myX + (myY * layer1Size) + (myZ * layer1Size * layer1Size);

	assert(myItemIndex < layer1Size * layer1Size * layer1Size);

	aItem.filled = 1.f;
	mychunk.items[myItemIndex] = aItem;*/
}

size_t VoxelGrid::getGridSize() const
{
	return gridLayer1DataSize;
}

const void* VoxelGrid::getGridData() const
{
	return gridLayer1Data;
}

const void* VoxelGrid::getLayer1ChunkData() const
{
	assert(layer1Chunks.size() > 0); //can't use empty octree

	return &layer1Chunks[0];
}

size_t VoxelGrid::getLayer1ChunkDataSize() const
{
	return layer1Chunks.size();
}

const void* VoxelGrid::getLayer2ChunkData() const
{
	assert(layer2Chunks.size() > 0); //can't use empty octree

	return &layer2Chunks[0];
}

size_t VoxelGrid::getLayer2ChunkDataSize() const
{
	return layer2Chunks.size();
}

int VoxelGrid::getSizeX() const
{
	return sizeX;
}

int VoxelGrid::getSizeY() const
{
	return sizeY;
}

int VoxelGrid::getSizeZ() const
{
	return sizeZ;
}

Layer1Chunk::Layer1Chunk()
{
	for (auto& index : itemIndices)
	{
		index = -1;
	}
}
