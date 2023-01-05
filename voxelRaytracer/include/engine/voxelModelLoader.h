#pragma once
#include "engine\voxelModel.h"

class VoxelModelLoader
{
public:
	static VoxelModel* getModel(const char* aFileName, int aResolution);

};

