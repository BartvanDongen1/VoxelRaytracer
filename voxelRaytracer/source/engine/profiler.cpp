#include "engine/profiler.h"

void Profiler::startProfiling()
{
	clearData();
	isProfiling = true;
}

void Profiler::stopProfiling()
{
	isProfiling = false;
}

void Profiler::update(float aDeltaTime)
{
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

void Profiler::clearData()
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
