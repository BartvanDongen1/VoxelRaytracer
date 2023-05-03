#include "engine/profiler.h"

Profiler::~Profiler()
{
	for (const auto& item : profileValues)
	{
		delete item.second;
	}
}

void Profiler::startProfiling()
{
	clearData();
	isProfiling = true;
}

void Profiler::stopProfiling()
{
	isProfiling = false;
}

void Profiler::addProfileValue(std::string aName)
{
	ProfileValue* myProfileValue = new ProfileValue();
	myProfileValue->name = aName;

	profileValues.insert({ aName, myProfileValue });
}

void Profiler::updateProfileValue(std::string aName, float aValue)
{
	if (isProfiling)
	{
		if (profileValues.count(aName) == 0)
		{
			addProfileValue(aName);
		}

		ProfileValue* myProfileValue = profileValues.find(aName)->second;

		if (currentTimestep < FRAME_TIME_ARRAY_SIZE)
		{
			myProfileValue->totalValue += aValue;
			myProfileValue->currentValue += aValue;

			myProfileValue->accumulatedValues++;


			if (timestepAccumulatedTime > 1.f / PLOT_WRITES_PER_SECOND)
			{
				float myAverageValue = myProfileValue->currentValue / myProfileValue->accumulatedValues;
				myProfileValue->values[currentTimestep] = myAverageValue;

				myProfileValue->currentValue = 0.f;
				myProfileValue->accumulatedValues = 0;
			}
		}

		profileValues[aName] = myProfileValue;
	}
}

void Profiler::update(float aDeltaTime)
{
	if (!isProfiling) return;

	accumulatedProflingFrames++;
	accumulatedProflingTime += aDeltaTime;
	timestepAccumulatedFrames++;

	if (!(currentTimestep < FRAME_TIME_ARRAY_SIZE)) return;

	if (timestepAccumulatedTime > 1.f / PLOT_WRITES_PER_SECOND)
	{
		timestepAccumulatedTime -= 1.f / PLOT_WRITES_PER_SECOND;
		currentTimestep++;
		timestepAccumulatedFrames = 0;
	}

	timestepAccumulatedTime += aDeltaTime;
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

	for (const auto& item : profileValues)
	{
		delete item.second;
	}
	profileValues.clear();

	timestepAccumulatedFrames = 0;
	timestepAccumulatedTime = 0.f;
	currentTimestep = 0;
}
