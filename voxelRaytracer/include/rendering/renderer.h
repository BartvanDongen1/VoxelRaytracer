#pragma once
#include "rendering\octree.h"

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

	void init();
	void update(float aDeltaTime);
	void shutdown();
private:
	int framesThisSecond{ 0 };
	float frameTimeAccumilator{ 0.f };

	int fps{ 0 };

	Graphics* graphics;
	
	VoxelModel* scene;

	Controller* cameraController;
	Camera* camera;
	//octree::OctreeChunk octreeChunk;

	float offset = 0;

	bool windowFocused{ false };
};

