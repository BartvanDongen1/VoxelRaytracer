#include "rendering\renderer.h"
#include "engine\inputManager.h"
#include "window.h"
#include "engine\timer.h"

int main()
{
	Timer myTimer;

	Renderer myRenderer;
	InputManager::init();

	myRenderer.init();
	bool shouldClose = false;
	while (!Window::getShouldClose())
	{
		float myDeltaTime = static_cast<float>(myTimer.getFrameTime());

		myRenderer.update(myDeltaTime);
	}

	myRenderer.shutdown();

	return 0;
}