#pragma once
#include <chrono>

class Timer
{
public:
	void reset();
	double getFrameTime();
	double getTotalTime();

private:
	std::chrono::time_point<std::chrono::steady_clock> startTime{ std::chrono::high_resolution_clock::now() };
	std::chrono::time_point<std::chrono::steady_clock> currentTime{ std::chrono::high_resolution_clock::now() };
};

static Timer worldTimer;
