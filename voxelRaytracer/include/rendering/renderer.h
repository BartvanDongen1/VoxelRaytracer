#pragma once
#include "rendering\octree.h"
#include "rendering\imguiWindowManager.h"

class Graphics;
class Window;
class Camera;
class Controller;
struct VoxelModel;
class Octree;

class Renderer
{
public:
	Renderer();
	~Renderer();

	void init();
	void update(float aDeltaTime);
	void shutdown();
private:
	Graphics* graphics;
	
	VoxelModel* scene;

	Controller* cameraController;
	Camera* camera;
	Octree* octree;

	ImguiWindowManager imguiWindow;

	float offset = 0;

	bool windowFocused{ false };
};