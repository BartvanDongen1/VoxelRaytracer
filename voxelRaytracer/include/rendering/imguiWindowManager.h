#pragma once
#include "graphics.h"

#define FRAME_TIME_ARRAY_SIZE 1000
#define PLOT_WRITES_PER_SECOND 5

class ImguiWindowManager
{

public:
	void updateAndRender(const Graphics& aGraphics, float aDeltaTime);

private:
	void updateVariables(const Graphics& aGraphics, float aDeltaTime);
	void clearProfilingVariables();
	void plotProfilingData();

	//fps stuff
	int framesThisSecond{ 0 };
	int totalFrames{ 0 };
	float frameTimeAccumilator{ 0.f };

	int fps{ 0 };

	int framesAccumulated{ 0 };

	bool profilerOpen{ false };

	//profiling
	bool isProfiling{ false };
	int accumulatedProflingFrames{ 0 };
	float accumulatedProflingTime{ 1.f };

	int currentTimestep{ 0 };
	int timestepAccumulatedFrames{ 0 };
	float timestepAccumulatedTime{ 1.f };

	float averageFrameTimeArray[FRAME_TIME_ARRAY_SIZE]{ 0 };
	float timeArray[FRAME_TIME_ARRAY_SIZE]{ 0 };
};

