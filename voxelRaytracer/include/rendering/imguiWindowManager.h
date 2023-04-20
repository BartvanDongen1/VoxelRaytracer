#pragma once
#include "graphics.h"
#include "engine\benchmarkManager.h"
#include "engine\profiler.h"

#define FRAME_TIME_ARRAY_SIZE 1000
#define PLOT_WRITES_PER_SECOND 5

class ImguiWindowManager
{
public:
	ImguiWindowManager();
	~ImguiWindowManager();

	void updateAndRender(const Graphics& aGraphics, float aDeltaTime);
	void setController(Controller* aController);

private:
	void update(const Graphics& aGraphics, float aDeltaTime);
	void plotProfilingData();

	void benchmarkEndCallback();

	//benchmark
	BenchmarkManager* benchmarkManager{ nullptr };
	bool benchmarkToolOpen{ false };
	bool benchmarkRecordingWindowOpen{ false };

	int benchmarkRecordingNameSize{ 16 };
	char benchmarkRecordingName[16]{ "" };

	std::string selectedBenchmarkFileName{ "" };
	int currentPartIdx{ 0 };

	bool benchmarkNameHasError{ false };
	std::string benchmarkNameErrorMessage;

	//fps stuff
	int framesThisSecond{ 0 };
	int totalFrames{ 0 };
	float frameTimeAccumilator{ 0.f };

	int fps{ 0 };

	int framesAccumulated{ 0 };

	bool profilerOpen{ false };

	Profiler* profiler;
};

