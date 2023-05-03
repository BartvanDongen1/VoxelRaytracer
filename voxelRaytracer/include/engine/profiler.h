#pragma once
#include <string>
#include <unordered_map>

#define FRAME_TIME_ARRAY_SIZE 1000
#define PLOT_WRITES_PER_SECOND 5

class ImguiWindowManager;

struct ProfileValue
{
	std::string name{ "" };

	float totalValue{ 0 };

	int accumulatedValues{ 0 };
	float currentValue{ 0 };

	float values[FRAME_TIME_ARRAY_SIZE]{ 0 };
};

class Profiler
{
	friend class ImguiWindowManager;
public:
	Profiler() {};
	~Profiler();

	void startProfiling();
	void stopProfiling();

	void addProfileValue(std::string aName);
	void updateProfileValue(std::string aName, float aValue);

	void update(float aDeltaTime);

	void clearData();

private:
	//profiling
	bool isProfiling{ false };
	int accumulatedProflingFrames{ 0 };
	float accumulatedProflingTime{ 1.f };

	int currentTimestep{ 0 };
	int timestepAccumulatedFrames{ 0 };
	float timestepAccumulatedTime{ 1.f };

	float averageFrameTimeArray[FRAME_TIME_ARRAY_SIZE]{ 0 };
	float timeArray[FRAME_TIME_ARRAY_SIZE]{ 0 };

	//seperate profile values
	std::unordered_map<std::string, ProfileValue*> profileValues;
};

