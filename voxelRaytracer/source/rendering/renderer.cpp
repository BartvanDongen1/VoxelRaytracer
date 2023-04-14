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

	octree = new Octree();

	scene = new VoxelModel(16, 16, 16);
	//initRandomVoxels(scene, 5);
	//initFilled(scene);
	scene->data[0] = 1;
	scene->data[15] = 1;
	scene->data[240] = 1;
	scene->data[255] = 1;
	scene->data[3840] = 1;
	scene->data[3855] = 1;
	scene->data[4080] = 1;
	scene->data[4095] = 1;


	//VoxelModel* myTeapot = VoxelModelLoader::getModel("resources/models/teapot/teapot.obj", 16);
	//VoxelModel* myMonkey = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 16);
	//scene = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 16);

	Texture* myTexture = VoxelModelLoader::getTexture("resources/textures/blueNoise.png");
	graphics->updateNoiseTexture(*myTexture);

	octree->init(scene);

	octree2 = new Octree2();
	octree2->init(scene);

	//Octree2* myOctree = new Octree2();
	//myOctree->init(scene);

	graphics->updateOctreeVariables(*octree2);

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

	graphics->updateCameraVariables(*camera, windowFocused, octree2->getSize());
	graphics->updateAccumulationVariables(windowFocused);

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