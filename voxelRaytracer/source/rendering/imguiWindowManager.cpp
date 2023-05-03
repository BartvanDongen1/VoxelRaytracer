#include "rendering/imguiWindowManager.h"
#include "rendering/imgui-docking/imgui.h"
#include "rendering/imgui-docking/implot.h"
#include "engine\logger.h"

using namespace ImGui;
using namespace ImPlot;

ImguiWindowManager::ImguiWindowManager()
{
	benchmarkManager = new BenchmarkManager();
	profiler = new Profiler();

	benchmarkManager->setProfiler(profiler);
}

ImguiWindowManager::~ImguiWindowManager()
{
	delete benchmarkManager;
	delete profiler;
}

void ImguiWindowManager::updateAndRender(const Graphics& aGraphics, float aDeltaTime)
{
	update(aGraphics, aDeltaTime);

	bool windowOpen = true;

	Begin("settings", &windowOpen, ImGuiWindowFlags_None);
	Text("current FPS: %i", fps);
	Text("renderTime (MS): %f", aDeltaTime * 1000.f);
	Text("accumulated frames: %i", framesAccumulated);
	
	for (const auto& item : gpuProfiler->GetProfilerResults())
	{
		Text("%s: %f", item.name.c_str(), item.timeMS);
	}

	if (Button("profiler"))
	{
		profilerOpen = true;
	}

	if (Button("benchmark tool"))
	{
		benchmarkToolOpen = true;
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
			profiler->startProfiling();
		}

		SameLine();

		if (Button("stop profiling"))
		{
			profiler->stopProfiling();
		}

		//fps
		if (profiler->profileValues.count("GameLoop"))
		{
			ProfileValue* myValue = profiler->profileValues["GameLoop"];

			float myTotalFramesSeconds = myValue->totalValue / 1000.f;

			float averageFPS = profiler->accumulatedProflingFrames / myTotalFramesSeconds;

			Text("Average FPS: %.3f", averageFPS);

			double myRaysPerSecond = averageFPS * 1920 * 1080;

			Text("Rays per second: %.3f Million", myRaysPerSecond / 1000000.f);
		}

		Text(""); // blank line

		//frame count
		Text("Frame count: %i", profiler->accumulatedProflingFrames);

		Text(""); // blank line

		plotProfilingData();

		End();
	}

	if (benchmarkToolOpen)
	{
		Begin("Benchmark tool", &profilerOpen, ImGuiWindowFlags_None);

		if (Button("close"))
		{
			benchmarkToolOpen = false;

			benchmarkManager->stopBenchmark();
		}

		SameLine();

		if (Button("start benchmark"))
		{
			if (benchmarkManager->startBenchmark(selectedBenchmarkFileName))
			{
				profilerOpen = true;
			}
		}

		SameLine();

		if (Button("record benchmark"))
		{
			benchmarkRecordingWindowOpen = true;
			benchmarkManager->startBenchmarkRecording();
		}

		{
			// benchmark list
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
			const auto childSize = ImVec2(310, 280);
			ImGui::BeginChild("BenchmarkList", childSize, true, ImGuiWindowFlags_MenuBar);

			if (ImGui::BeginMenuBar()) {
				ImGui::TextUnformatted("saved benchmarks");
				ImGui::EndMenuBar();
			}

			const auto  listSize = ImVec2(childSize[0], childSize[1] - 20);
			
			const auto items = benchmarkManager->getBenchmarkFileNames();


			if (ImGui::BeginListBox("##benchmark_list", listSize)) {
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
					if (currentPartIdx < items.size() - 1) { ++currentPartIdx; }
				}
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
					if (currentPartIdx > 0) { --currentPartIdx; }
				}

				int i = 0;
				for (const auto& item : items)
				{
					const bool is_selected = (currentPartIdx == i);
					if (ImGui::Selectable(item.c_str(), is_selected)) 
					{ 
						currentPartIdx = i;
						selectedBenchmarkFileName = item.c_str();
					}

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (is_selected) { ImGui::SetItemDefaultFocus(); }

					++i;
				}

				ImGui::EndListBox();
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
		}

		if (Button("refresh list"))
		{
			benchmarkManager->updateBenchmarkFileNames();
		}

		End();
	}

	if (benchmarkRecordingWindowOpen)
	{
		if (Button("stop"))
		{
			benchmarkManager->stopBenchmarkRecording();
		}

		SameLine();

		if (Button("cancel"))
		{
			benchmarkManager->stopBenchmarkRecording();
			benchmarkManager->deleteRecordingData();
			benchmarkRecordingWindowOpen = false;
			benchmarkNameHasError = false;
		}

		SameLine();

		if (Button("save"))
		{
			LOG_INFO("Atempting to save benchmark with name: %s", benchmarkRecordingName);

			std::string myFileName = benchmarkRecordingName;
			myFileName.append(".json");

			//check if name is filled in
			if (benchmarkRecordingName[0] == '\0')
			{
				benchmarkNameHasError = true;
				benchmarkNameErrorMessage = "You must fill in a name for the benchmark";
			}
			//check if name isn't dupicate
			else if (benchmarkManager->getBenchmarkFileNames().count(myFileName))
			{
				benchmarkNameHasError = true;
				benchmarkNameErrorMessage = "A file already exists with that name";
			}
			else
			{
				//save file
				benchmarkManager->saveBenchmark(myFileName);

				//clear benchmark data
				benchmarkManager->deleteRecordingData();
				benchmarkRecordingWindowOpen = false;
				benchmarkNameHasError = false;
			}
		}

		PushItemWidth(140);
		InputText("##text1", benchmarkRecordingName, benchmarkRecordingNameSize);

		if (benchmarkNameHasError)
		{
			SameLine();
			Text(benchmarkNameErrorMessage.c_str());
		}

		Text("recording duration: %.3f", benchmarkManager->getRecordingDuration());
		Text("capture count: %i", benchmarkManager->getRecordingCaptureCount());
	}
}

void ImguiWindowManager::setController(Controller* aController)
{
	benchmarkManager->setController(aController);
}

void ImguiWindowManager::setGpuProfiler(GPUProfiler* aGpuProfiler)
{
	gpuProfiler = aGpuProfiler;
}

void ImguiWindowManager::update(const Graphics& aGraphics, float aDeltaTime)
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
	profiler->updateProfileValue("GameLoop", aDeltaTime * 1000.f);
	for (const auto& item : gpuProfiler->GetProfilerResults())
	{
		profiler->updateProfileValue(item.name, item.timeMS);
	}

	profiler->update(aDeltaTime);

	//update benchmark manager
	benchmarkManager->update(aDeltaTime);
}

void ImguiWindowManager::plotProfilingData()
{
	if (BeginPlot("Frame Times"))
	{
		SetupAxes("Time (seconds)", "Frame Time (MS)");
		SetupAxesLimits(0, 30, 0, 50);

		//PlotLine("GameLoop", profiler->timeArray, profiler->averageFrameTimeArray, profiler->currentTimestep);
		
		for (const auto& item : profiler->profileValues)
		{
			PlotLine(item.first.c_str(), profiler->timeArray, item.second->values, profiler->currentTimestep);
		}
		
		SetNextMarkerStyle(ImPlotMarker_Circle);
		EndPlot();
	}
}

void ImguiWindowManager::benchmarkEndCallback()
{
	profiler->stopProfiling();
}
