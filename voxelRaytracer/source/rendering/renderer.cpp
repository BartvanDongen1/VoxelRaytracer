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
	Window::init(aSizeX, aSizeY, "Voxel Renderer");
	graphics->init(aSizeX, aSizeY);
	
	cameraController->init();
	camera = cameraController->getCamera();
	imguiWindow.setController(cameraController);
	imguiWindow.setGpuProfiler(graphics->getProfiler());

	octree = new Octree();
	
	//top level scene
	VoxelModel myMainScene = VoxelModel(128, 128, 128);

	//place floor
	VoxelModel myFloor = VoxelModel(128, 1, 128);
	initFilled(&myFloor, 2);

	myMainScene.combineModel(0, 109, 0, &myFloor);


	//scene = VoxelModelLoader::getModel("resources/models/teapot/teapot.obj", 16);
	//scene = VoxelModelLoader::getModel("resources/models/monkey/monkey.obj", 128);
	scene = VoxelModelLoader::getModel("resources/models/dragon/dragon.obj", 128, 1);

	myMainScene.combineModel(0, 20, 0, scene);

	//scene->combineModel(0, 89, 0, &myFloor);

	//scene = new VoxelModel(128, 128, 128);
	//initFilled(scene);

	//initRandomVoxels(scene, 100);
	//initRandomVoxels(scene, 1, 2000);

	placeFilledSphere(&myMainScene, 30, 50, 100, 14, 3);
	placeFilledSphere(&myMainScene, 100, 20, 30, 14, 4);

	Texture* myTexture = VoxelModelLoader::getTexture("resources/textures/blueNoise.png");
	graphics->updateNoiseTexture(*myTexture);

	//Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/studio.hdr");
	Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/midday.hdr");
	//Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/sunset.hdr");
	//Texture* mySkyDomeTexture = VoxelModelLoader::getHdrTexture("resources/textures/skydomes/alps.hdr");
	graphics->updateSkydomeTexture(*mySkyDomeTexture);

	octree = new Octree();
	voxelGrid = new VoxelGrid();

	octree->init(&myMainScene);
	voxelGrid->init(&myMainScene);

	{
		VoxelAtlasItem myItem;

	//voxel 0: empty
		voxelAtlas->addItem(myItem);

	// voxel 1: white material
		myItem.colorAndRoughness = glm::vec4(0.8, 0.8, 0.8, 1.f);
		myItem.specularAndPercent = glm::vec4(0.9, 0.9, 0.9, 0.0f);

		voxelAtlas->addItem(myItem);

	// voxel 2: gray material
		myItem.colorAndRoughness = glm::vec4(0.2, 0.2, 0.2, 0.1f);
		myItem.specularAndPercent = glm::vec4(0.4, 0.4, 0.4, 0.95f);

		voxelAtlas->addItem(myItem);

	//voxel 3: red light
		myItem.colorAndRoughness = glm::vec4(50, 0.5, 0.5, 0.f);
		myItem.isLight = 1;
		voxelAtlas->addItem(myItem);

	//voxel 4: green light
		myItem.colorAndRoughness = glm::vec4(0.5, 50, 0.5, 0.f);
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