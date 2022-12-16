#pragma once

class Graphics;
class Window;
class Camera;

class Renderer
{
public:
	Renderer();
	~Renderer();

	void init();
	void update(float aDeltaTime);
	void shutdown();
private:
	int framesThisSecond{ 0 };
	float frameTimeAccumilator{ 0.f };

	int fps{ 0 };

	Graphics* graphics;
	Camera* camera;
};

