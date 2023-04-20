#include "engine/benchmarkManager.h"

#include <fstream>
#include <iostream>
#include <filesystem>

#include "engine\logger.h"
#include "engine\controller.h"
#include "engine\profiler.h"

constexpr const char* benchmarkPath = "saves/benchmarks/";

namespace fs = std::filesystem;
using namespace nlohmann;

void BenchmarkManager::setProfiler(Profiler* aProfiler)
{
	profiler = aProfiler;
}

void BenchmarkManager::update(float aDeltaTime)
{
	if (recordingPendingDeletion)
	{
		clearBenchmarkRecordingValues();
		recordingPendingDeletion = false;
	}

	if (isRecording)
	{
		updateBenchmarkRecording(aDeltaTime);
	}

	if (isBenchmarking)
	{
		updateBenchmark(aDeltaTime);
	}
}

void BenchmarkManager::setController(Controller* aController)
{
	controller = aController;
}

bool BenchmarkManager::startBenchmark(std::string aBenchmarkName)
{
	if (aBenchmarkName == "")
	{
		LOG_ERROR("benchmark file name is blank");
		return false;
	}

	LOG_INFO("starting benchmark for file: %s", aBenchmarkName.c_str());

	// dissable camera controls
	controller->disableInputs();

	runningBenchmark = new BenchmarkJson();

	//read json file to struct
	std::string myFilePath = benchmarkPath;
	myFilePath.append(aBenchmarkName);

	if (!fs::exists(myFilePath))
	{
		LOG_ERROR("benchmark file doesn't exist: %s", myFilePath.c_str());
		return false;
	}

	std::ifstream myParsedData(myFilePath);
	json myParsedJson = json::parse(myParsedData);

	runningBenchmark->name = myParsedJson["name"].get<std::string>();
	runningBenchmark->captureInterval = myParsedJson["captureInterval"].get<float>();
	runningBenchmark->duration = myParsedJson["duration"].get<float>();

	for (const auto& element : myParsedJson["captures"])
	{
		BenchmarkListElement myElement;

		myElement.positionX = element["x"].get<float>();
		myElement.positionY = element["y"].get<float>();
		myElement.positionZ = element["z"].get<float>();

		myElement.directionX = element["dirX"].get<float>();
		myElement.directionY = element["dirY"].get<float>();
		myElement.directionZ = element["dirZ"].get<float>();

		runningBenchmark->list.push_back(myElement);
	}

	myParsedData.close();

	// enable benchmark update function
	isBenchmarking = true;

	profiler->startProfiling();

	return true;
}

void BenchmarkManager::stopBenchmark()
{
	delete runningBenchmark;
	isBenchmarking = false;
	elapsedTime = 0.f;
	controller->enableInputs();

	profiler->stopProfiling();
}

#define LERP(a, b, x) a + x * (b - a)

void BenchmarkManager::updateBenchmark(float aDeltaTime)
{
	if (elapsedTime > runningBenchmark->duration)
	{
		stopBenchmark();
		return;
	}

	int index = static_cast<int>(elapsedTime / runningBenchmark->captureInterval);
	float intervalLerpValue = fmod(elapsedTime, runningBenchmark->captureInterval) / runningBenchmark->captureInterval;
	
	BenchmarkListElement myElement1 = runningBenchmark->list[index];
	BenchmarkListElement myElement2 = runningBenchmark->list[index + 1];

	glm::vec3 myPosition;
	myPosition.x = LERP(myElement1.positionX, myElement2.positionX, intervalLerpValue);
	myPosition.y = LERP(myElement1.positionY, myElement2.positionY, intervalLerpValue);
	myPosition.z = LERP(myElement1.positionZ, myElement2.positionZ, intervalLerpValue);

	glm::vec3 myDirection;
	myDirection.x = LERP(myElement1.directionX, myElement2.directionX, intervalLerpValue);
	myDirection.y = LERP(myElement1.directionY, myElement2.directionY, intervalLerpValue);
	myDirection.z = LERP(myElement1.directionZ, myElement2.directionZ, intervalLerpValue);

	controller->camera->position = myPosition;
	controller->camera->setDirection(myDirection);

	elapsedTime += aDeltaTime;
}

void BenchmarkManager::startBenchmarkRecording()
{
	clearBenchmarkRecordingValues();

	recording = new BenchmarkJson();

	recording->captureInterval = captureInterval;

	isRecording = true;
}

void BenchmarkManager::stopBenchmarkRecording()
{
	isRecording = false;

	recording->duration = recordingDuration;
}

void BenchmarkManager::deleteRecordingData()
{
	recordingPendingDeletion = true;
}

void BenchmarkManager::clearBenchmarkRecordingValues()
{
	if (recording != nullptr)
	{
		delete recording;
		recording = nullptr;
	}

	isRecording = false;
	recordingDuration = 0.f;
	deltaCaptureTime = 0.f;
}

void BenchmarkManager::updateBenchmarkRecording(float aDeltaTime)
{
	//we assume we are recording a benchmark in the function

	recordingDuration += aDeltaTime;

	if (deltaCaptureTime <= 0)
	{
		deltaCaptureTime += captureInterval;

		//save a capture
		glm::vec3 myPosition = controller->getCamera()->position;
		glm::vec3 myDirection = controller->getCamera()->getDirection();

		BenchmarkListElement myElement;

		myElement.positionX = myPosition.x;
		myElement.positionY = myPosition.y;
		myElement.positionZ = myPosition.z;

		myElement.directionX = myDirection.x;
		myElement.directionY = myDirection.y;
		myElement.directionZ = myDirection.z;

		recording->list.push_back(myElement);
	}

	deltaCaptureTime -= aDeltaTime;
}

void BenchmarkManager::saveBenchmark(std::string aFileName)
{
	assert(recording != nullptr); // can't save empty benchmark
	assert(recording->captureInterval == captureInterval); // capture interval should not change while recording

	recording->name = aFileName.substr(0, aFileName.size() - 5); // don't store the ".json" in the recording name

	//save struct to json format
	json myJsonFile;

	myJsonFile["name"] = recording->name;
	myJsonFile["duration"] = recording->duration - fmod(recording->duration, recording->captureInterval);
	myJsonFile["captureInterval"] = recording->captureInterval;

	for (int i = 0; i < recording->list.size(); i++)
	{
		json myCapture;
		myCapture["x"] = recording->list[i].positionX;
		myCapture["y"] = recording->list[i].positionY;
		myCapture["z"] = recording->list[i].positionZ;
		myCapture["dirX"] = recording->list[i].directionX;
		myCapture["dirY"] = recording->list[i].directionY;
		myCapture["dirZ"] = recording->list[i].directionZ;

		myJsonFile["captures"].push_back({ myCapture });
	}

	std::string string = myJsonFile.dump(4);

	std::string myOutputPath = benchmarkPath;
	myOutputPath.append(aFileName);

	std::ofstream outputFile(myOutputPath);
	outputFile << string;
	outputFile.close();
}

void BenchmarkManager::updateBenchmarkFileNames()
{
	benchmarkNames.clear();

	if (fs::exists(benchmarkPath))
	{
		LOG_INFO("Found benchmark files: ");

		auto test = fs::directory_iterator(benchmarkPath);

		for (const auto& entry : test)
		{
			std::string stringName = entry.path().filename().string();
			
			LOG_INFO(stringName.c_str());

			benchmarkNames.insert(stringName);
		}
	}
	else
	{
		LOG_WARNING("benchmark file directory not found");
	}
}

int BenchmarkManager::getRecordingCaptureCount() const
{
	assert(recording != nullptr); // recording has to be existing

	return static_cast<int>(recording->list.size());
}

float BenchmarkManager::getRecordingDuration() const
{
	assert(recording != nullptr); // recording has to be existing

	return recordingDuration;
}

const std::unordered_set<std::string>& BenchmarkManager::getBenchmarkFileNames() const
{
	return benchmarkNames;
}
