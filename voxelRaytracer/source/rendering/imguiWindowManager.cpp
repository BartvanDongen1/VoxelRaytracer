#include "rendering/imguiWindowManager.h"
#include "rendering/imgui-docking/imgui.h"
#include "rendering/imgui-docking/implot.h"

using namespace ImGui;
using namespace ImPlot;

void ImguiWindowManager::updateAndRender(const Graphics& aGraphics, float aDeltaTime)
{
	updateVariables(aGraphics, aDeltaTime);

	bool windowOpen = true;

	Begin("settings", &windowOpen, ImGuiWindowFlags_None);
	Text("current FPS: %i", fps);
	Text("renderTime (MS): %f", aDeltaTime * 1000.f);
	Text("accumulated frames: %i", framesAccumulated);
	
	if (Button("profiler"))
	{
		profilerOpen = true;
	}

	End();

	if (profilerOpen)
	{
		Begin("Profiler", &profilerOpen, ImGuiWindowFlags_None);
		
		if (Button("close"))
		{
			profilerOpen = false;
		}

		SameLine();

		if (Button("start profiling"))
		{
			clearProfilingVariables();
			isProfiling = true;
		}

		SameLine();

		if (Button("stop profiling"))
		{
			isProfiling = false;
		}

		Text("average FPS: %.3f", accumulatedProflingFrames / accumulatedProflingTime);
		Text("frame count: %i", accumulatedProflingFrames);

		plotProfilingData();

		End();
	}
}

void ImguiWindowManager::updateVariables(const Graphics& aGraphics, float aDeltaTime)
{
	//fps counter
	frameTimeAccumilator += aDeltaTime;
	framesThisSecond++;
	totalFrames++;

	if (frameTimeAccumilator > 0.2f)
	{
		frameTimeAccumilator -= 0.2f;
		fps = framesThisSecond * 5;
		framesThisSecond = 0;
	}

	//frame accumulator
	framesAccumulated = aGraphics.accumulationConstantBuffer->framesAccumulated;


	//profiling varaibles
	if (isProfiling)
	{
		accumulatedProflingFrames++;
		accumulatedProflingTime += aDeltaTime;

		timestepAccumulatedFrames++;
		timestepAccumulatedTime += aDeltaTime;

		if (currentTimestep < FRAME_TIME_ARRAY_SIZE)
		{
			if (timestepAccumulatedTime > 1.f / PLOT_WRITES_PER_SECOND)
			{
				float myAverageDeltaTime = timestepAccumulatedTime / timestepAccumulatedFrames;
				averageFrameTimeArray[currentTimestep++] = myAverageDeltaTime * 1000.f;

				timestepAccumulatedTime -= 1.f / PLOT_WRITES_PER_SECOND;
				timestepAccumulatedFrames = 0;
			}
		}
	}
}

void ImguiWindowManager::clearProfilingVariables()
{
	accumulatedProflingFrames = 0;
	accumulatedProflingTime = 0.f;

	for (int i = 0; i < FRAME_TIME_ARRAY_SIZE; i++)
	{
		averageFrameTimeArray[i] = 0.f;
		timeArray[i] = static_cast<float>(i) / PLOT_WRITES_PER_SECOND;
	}

	timestepAccumulatedFrames = 0;
	timestepAccumulatedTime = 0.f;
	currentTimestep = 0;
}

void ImguiWindowManager::plotProfilingData()
{
	if (BeginPlot("Frame Times"))
	{
		SetupAxes("Time (seconds)", "Frame Time (MS)");
		SetupAxesLimits(0, 30, 0, 50);
		PlotLine("Total Render Time", timeArray, averageFrameTimeArray, currentTimestep);
		SetNextMarkerStyle(ImPlotMarker_Circle);
		EndPlot();
	}
}
