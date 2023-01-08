#include "rendering\renderer.h"
#include "rendering\graphics.h"
#include "rendering\camera.h"
#include "window.h"
#include "engine\controller.h"
#include "engine\inputManager.h"
#include "engine\voxelModelLoader.h"
#include "rendering\octree.h"

Renderer::Renderer()
{
	graphics = new Graphics();
	cameraController = new Controller();
}

Renderer::~Renderer()
{
	delete graphics;
	delete cameraController;
}

void Renderer::init()
{
	Window::init(1920, 1080, "Voxel Renderer");
	graphics->init();
	
	cameraController->init();
	camera = cameraController->getCamera();

	octree = new Octree({ 0,0,0 }, { 32,32,32 });

	scene = new VoxelModel(32, 32, 32);
	//initRandomVoxels(scene, 5);
	VoxelModel* myTeapot = VoxelModelLoader::getModel("resources/models/teapot/teapot.obj", 32);
	VoxelModel* myMonkey = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 32);
	//scene = VoxelModelLoader::getModel("resources/models/sponza/sponza.obj", 32);

	octree->init(myMonkey);

	scene->combineModel(0, 0, 0, myMonkey);
}

void Renderer::update(float aDeltaTime)
{
	offset += aDeltaTime / 10;

	Window::processMessages();

	// controller updates
	if (InputManager::getKey(Keys::f)->pressed)
	{
		if (windowFocused)
		{
			Window::freeCursor();
			Window::showCursor();
		}
		else
		{
			Window::confineCursor();
			Window::hideCursor();
		}

		windowFocused = !windowFocused;
	}

	if (windowFocused)
	{
		// only update controller if window in focus
		cameraController->update(aDeltaTime);
	}

	//fps counter
	frameTimeAccumilator += aDeltaTime;
	framesThisSecond++;

	if (frameTimeAccumilator > 0.2f)
	{
		frameTimeAccumilator -= 0.2f;
		fps = framesThisSecond * 5;
		framesThisSecond = 0;
	}

	graphics->updateOctreeVariables(*scene);
	graphics->updateCameraVariables(*camera);

	//rendering
	graphics->beginFrame();


	graphics->renderFrame();
	graphics->copyComputeTextureToBackbuffer();

	graphics->renderImGui(fps);

	graphics->endFrame();

}

void Renderer::shutdown()
{
	graphics->shutdown();
}