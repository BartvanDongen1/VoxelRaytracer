#include "rendering\renderer.h"
#include "rendering\graphics.h"
#include "window.h"

Renderer::Renderer()
{
	graphics = new Graphics();
}

Renderer::~Renderer()
{
	delete graphics;
}

void Renderer::init()
{
	Window::init(1920, 1080, "Voxel Renderer");
	graphics->init();
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