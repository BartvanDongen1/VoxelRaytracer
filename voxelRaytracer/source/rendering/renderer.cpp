#include "rendering\renderer.h"
#include "rendering\graphics.h"
#include "rendering\camera.h"
#include "window.h"

Renderer::Renderer()
{
	graphics = new Graphics();
	camera = new Camera();
}

Renderer::~Renderer()
{
	delete graphics;
	delete camera;
}

void Renderer::init()
{
	Window::init(1920, 1080, "Voxel Renderer");
	graphics->init();
	
	glm::vec3 pos{ 0,0,0 };
	glm::vec3 dir{ 0,0,1 };
	
	camera->init(pos, dir, 100.f);
}

void Renderer::update(float aDeltaTime)
{
	Window::processMessages();

	//fps counter
	frameTimeAccumilator += aDeltaTime;
	framesThisSecond++;

	if (frameTimeAccumilator > 0.2f)
	{
		frameTimeAccumilator -= 0.2f;
		fps = framesThisSecond * 5;
		framesThisSecond = 0;
	}

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