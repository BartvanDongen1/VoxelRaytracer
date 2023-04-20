#pragma once
#include <vector>
#include <unordered_set>
#include <string>
#include <nlohmann\json.hpp>

class Controller;
class Profiler;

struct BenchmarkListElement
{
	float positionX;
	float positionY;
	float positionZ;

	float directionX;
	float directionY;
	float directionZ;
};

struct BenchmarkJson
{
	std::string name;
	float duration;
	float captureInterval;
	std::vector<BenchmarkListElement> list;
};

class BenchmarkManager
{
public:
	BenchmarkManager() {};
	~BenchmarkManager() {};

	void setProfiler(Profiler* aProfiler);

	void update(float aDeltaTime);
	
	void setController(Controller* aController);

	bool startBenchmark(std::string aBenchmarkName);
	void stopBenchmark();

	void updateBenchmark(float aDeltaTime);

	void startBenchmarkRecording();
	void stopBenchmarkRecording();
	void deleteRecordingData();

	void updateBenchmarkRecording(float aDeltaTime);

	void saveBenchmark(std::string aFileName);

	void updateBenchmarkFileNames();

	int getRecordingCaptureCount() const;
	float getRecordingDuration() const;

	const std::unordered_set<std::string>& getBenchmarkFileNames() const;
private:
	void clearBenchmarkRecordingValues();

	std::unordered_set<std::string> benchmarkNames;

	Controller* controller{ nullptr };

	float captureInterval{ 0.1f };

	//benchmark running
	BenchmarkJson* runningBenchmark{ nullptr };
	bool isBenchmarking{ false };
	float elapsedTime{ 0.f };
	
	Profiler* profiler{ nullptr };

	// benchmark recording
	BenchmarkJson* recording{ nullptr };
	bool isRecording{ false };
	float recordingDuration{ 0.f };
	float deltaCaptureTime{ 0.f };
	bool recordingPendingDeletion{ false };
};

