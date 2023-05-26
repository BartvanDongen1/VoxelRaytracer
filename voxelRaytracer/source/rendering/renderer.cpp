#include "rendering\renderer.h"
#include "rendering\graphics.h"
#include "rendering\camera.h"
#include "window.h"
#include "engine\controller.h"
#include "engine\inputManager.h"
#include "engine\voxelModelLoader.h"
#include "rendering\octree.h"
#include "engine\timer.h"
#include "engine\logger.h"

Renderer::Renderer()
{
	graphics = new Graphics();
	cameraController = new Controller();
	voxelAtlas = new VoxelAtlas();
}

Renderer::~Renderer()
{
	delete graphics;
	delete cameraController;
	delete voxelAtlas;
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
	scene = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 128);

	//scene = new VoxelModel(128, 128, 128);
	//initFilled(scene);

	//initRandomVoxels(scene, 100);
	//initRandomVoxels(scene, 1, 2000);

	Texture* myTexture = VoxelModelLoader::getTexture("resources/textures/blueNoise.png");
	graphics->updateNoiseTexture(*myTexture);

	//Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/studio.hdr");
	//Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/midday.hdr");
	Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/sunset.hdr");
	//Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/alps.hdr");
	graphics->updateSkydomeTexture(*mySkyDomeTexture);

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

	{
		VoxelAtlasItem myItem;

		myItem.color = glm::vec3(0.5, 0.5, 0.5);
		voxelAtlas->addItem(myItem);

		//myItem.color = glm::vec3(0, 0.5, 0);
		voxelAtlas->addItem(myItem);

		//myItem.color = glm::vec3(0, 0.5, 0.5);
		voxelAtlas->addItem(myItem);

		//myItem.color = glm::vec3(0.5, 0, 0);
		voxelAtlas->addItem(myItem);

		//myItem.color = glm::vec3(0.5, 0, 0.5);
		voxelAtlas->addItem(myItem);

		//myItem.color = glm::vec3(0.5, 0.5, 0);
		voxelAtlas->addItem(myItem);

		//myItem.color = glm::vec3(0.5, 0.5, 0.5);
		voxelAtlas->addItem(myItem);

		myItem.color = glm::vec3(4, 2, 2);
		myItem.isLight = 1;
		voxelAtlas->addItem(myItem);
	}

	graphics->updateVoxelAtlasVariables(*voxelAtlas);

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
	imguiWindow.setWindowResolution(Window::getWidth(), Window::getHeight());

	imguiWindow.updateAndRender(*graphics, aDeltaTime); 
	graphics->renderImGui();							
	
	graphics->endFrame();
}

void Renderer::shutdown()
{
	graphics->shutdown();
}