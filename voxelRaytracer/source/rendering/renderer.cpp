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

	OctreeItem myItem;
	myItem.color = glm::vec3(1, 1, 1);

	Window::init(1920, 1080, "Voxel Renderer");
	graphics->init();
	
	cameraController->init();
	camera = cameraController->getCamera();
	imguiWindow.setController(cameraController);

	octree = new Octree();


	//scene = VoxelModelLoader::getModel("resources/models/teapot/teapot.obj", 16);
	scene = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 64);

	//scene = new VoxelModel(64, 64, 64);
	
	//initRandomVoxels(scene, 20);

	Texture* myTexture = VoxelModelLoader::getTexture("resources/textures/blueNoise.png");
	graphics->updateNoiseTexture(*myTexture);

	octree = new Octree();
	octree->init(scene);

	graphics->updateOctreeVariables(*octree);
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

	graphics->updateCameraVariables(*camera, windowFocused, static_cast<int>(octree->getSize()));
	graphics->updateAccumulationVariables(windowFocused || !cameraController->getInputsEnabled());

	//rendering
	graphics->beginFrame();


	graphics->renderFrame();
	graphics->copyAccumulationBufferToBackbuffer();

	//imgui
	imguiWindow.updateAndRender(*graphics, aDeltaTime);
	graphics->renderImGui();

	graphics->endFrame();

}

void Renderer::shutdown()
{
	graphics->shutdown();
}