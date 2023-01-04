#include "rendering\renderer.h"
#include "rendering\graphics.h"
#include "rendering\camera.h"
#include "window.h"
#include "engine\controller.h"
#include "engine\inputManager.h"

Renderer::Renderer()
{
	graphics = new Graphics();
	cameraController = new Controller();
	//camera = new Camera();
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

	scene = new VoxelModel(32, 32, 32);
	initRandomVoxels(scene, 5);
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