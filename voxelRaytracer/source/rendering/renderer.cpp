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

void Renderer::init(const unsigned int aSizeX, const unsigned int aSizeY)
{
	OctreeItem myItem;
	myItem.color = glm::vec3(1, 1, 1);

	Window::init(aSizeX, aSizeY, "Voxel Renderer");
	graphics->init(aSizeX, aSizeY);
	
	cameraController->init();
	camera = cameraController->getCamera();
	imguiWindow.setController(cameraController);
	imguiWindow.setGpuProfiler(graphics->getProfiler());

	octree = new Octree();

	//scene = VoxelModelLoader::getModel("resources/models/teapot/teapot.obj", 16);
	//scene = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 128);

	scene = new VoxelModel(128, 128, 128);
	//initFilled(scene);

	initRandomVoxels(scene, 100);
	//initRandomVoxels(scene, 500);

	Texture* myTexture = VoxelModelLoader::getTexture("resources/textures/blueNoise.png");
	graphics->updateNoiseTexture(*myTexture);

	octree = new Octree();
	voxelGrid = new VoxelGrid();

	octree->init(scene);
	voxelGrid->init(scene);

	/*GridItem myItem2;
	myItem2.color = glm::vec3(1, 1, 1);

	voxelGrid->insertItem(0 , 0 , 0 , myItem2);
	voxelGrid->insertItem(63, 0 , 0 , myItem2);
	voxelGrid->insertItem(0 , 63, 0 , myItem2);
	voxelGrid->insertItem(63, 63, 0 , myItem2);
	voxelGrid->insertItem(0 , 0 , 63, myItem2);
	voxelGrid->insertItem(63, 0 , 63, myItem2);
	voxelGrid->insertItem(0 , 63, 63, myItem2);
	voxelGrid->insertItem(63, 63, 63, myItem2);*/

	graphics->updateOctreeVariables(*octree);
	graphics->updateVoxelGridVariables(*voxelGrid);
}

void Renderer::update(float aDeltaTime)
{
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

	//update profiling data only after all rendering is done
	//imguiWindow.updateProfilingData();
}

void Renderer::shutdown()
{
	graphics->shutdown();
}