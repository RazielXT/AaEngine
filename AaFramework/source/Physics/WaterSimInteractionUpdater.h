#pragma once

#include "RenderObject/WaterSim/WaterSim.h"
#include <vector>

class PhysicsManager;

class WaterSimInteractionUpdater
{
public:

	void update(float dt, PhysicsManager& physics, WaterSim& water);

private:

	struct TrackedBody
	{
		UINT dynamicBodyIndex;
	};
	std::vector<TrackedBody> trackedBodies;
	bool waitingForResults = false;
};
