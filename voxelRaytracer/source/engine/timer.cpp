#include "engine/timer.h"

void Timer::reset()
{
    startTime = std::chrono::high_resolution_clock::now();
}

double Timer::getFrameTime()
{
    auto newTime = std::chrono::high_resolution_clock::now();
    double elapsed = (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(newTime - currentTime).count() / 1'000'000'000.0);
    currentTime = newTime;

    return elapsed;
}

double Timer::getTotalTime()
{
    auto newTime = std::chrono::high_resolution_clock::now();
    double elapsed = (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(newTime - startTime).count() / 1'000'000'000.0);

    return elapsed;
}
