#pragma once
#include "rendering\octree.h"
#include "rendering\voxelGrid.h"
#include "rendering\imguiWindowManager.h"

class Graphics;
class Window;
class Camera;
class Controller;
struct VoxelModel;

class Renderer
{
public:
	Renderer();
	~Renderer();

	void init(const unsigned int aSizeX, const unsigned int aSizeY);
	void update(float aDeltaTime);
	void shutdown();
private:
	Graphics* graphics;
	
	VoxelModel* scene{ nullptr };

	Controller* cameraController;
	Camera* camera{ nullptr };
	
	Octree* octree{ nullptr };
	VoxelGrid* voxelGrid{ nullptr };

	ImguiWindowManager imguiWindow;

	float offset = 0;

	bool windowFocused{ false };
};