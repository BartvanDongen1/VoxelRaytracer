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

	octree = new Octree();

	scene = new VoxelModel(16, 16, 16);
	initRandomVoxels(scene, 5);
	//VoxelModel* myTeapot = VoxelModelLoader::getModel("resources/models/teapot/teapot.obj", 16);
	//VoxelModel* myMonkey = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 16);
	//scene = VoxelModelLoader::getModel("resources/models/sponza/sponza.obj", 32);

	Texture* myTexture = VoxelModelLoader::getTexture("resources/textures/blueNoise.png");
	graphics->updateNoiseTexture(*myTexture);

	octree->init(scene);
	graphics->updateOctreeVariables(*octree);

	//scene->combineModel(0, 0, 0, myTeapot);
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
	totalFrames++;

	if (frameTimeAccumilator > 0.2f)
	{
		frameTimeAccumilator -= 0.2f;
		fps = framesThisSecond * 5;
		framesThisSecond = 0;
	}

	graphics->updateCameraVariables(*camera, totalFrames, windowFocused);
	graphics->updateAccumulationVariables(windowFocused);

	//rendering
	graphics->beginFrame();


	graphics->renderFrame();
	graphics->copyAccumulationBufferToBackbuffer();

	graphics->renderImGui(fps);

	graphics->endFrame();

}

void Renderer::shutdown()
{
	graphics->shutdown();
}