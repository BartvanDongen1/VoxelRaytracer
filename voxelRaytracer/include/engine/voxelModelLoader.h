#pragma once
#include "engine\voxelModel.h"
#include "engine\texture.h"

class VoxelModelLoader
{
public:
	static VoxelModel* getModel(const char* aFileName, int aResolution);

	static Texture* getTexture(const char* aFileName);

	static Texture* getHdrTexture(const char* aFileName);
};

