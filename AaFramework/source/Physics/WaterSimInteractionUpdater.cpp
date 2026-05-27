#include "WaterSimInteractionUpdater.h"
#include "PhysicsManager.h"
#include "Scene/RenderObject.h"

using namespace JPH;

void WaterSimInteractionUpdater::update(float dt, PhysicsManager& physics, WaterSim& water)
{
	auto bounds = water.getWaterBounds();

	// Try to consume results from previous frame's GPU query
	if (waitingForResults)
	{
		std::vector<WaterSim::InteractionResult> results;
		if (water.consumeInteractionResults(results))
		{
			auto& bodyInterface = physics.system->GetBodyInterface();
			UINT resultCount = (UINT)min(results.size(), trackedBodies.size());

			for (UINT i = 0; i < resultCount; i++)
			{
				auto& result = results[i];
				auto& tracked = trackedBodies[i];

				if (tracked.dynamicBodyIndex >= physics.dynamicBodies.size())
					continue;

				auto& body = physics.dynamicBodies[tracked.dynamicBodyIndex];

				if (!bodyInterface.IsActive(body.id))
					continue;

				// Apply buoyancy force if submerged (positive waterHeightDiff = below water)
				if (result.waterHeightDiff > 0)
				{
					float submersionFactor = min(result.waterHeightDiff, 1.0f);

					const float buoyancyForce = 15.0f * 4;
					const float waterDragCoeff = 2.0f * 2;
					const float waterCurrentForceCoeff = 5.0f * 5;

					// Buoyancy: upward force proportional to submersion
					Vec3 buoyancy = Vec3(0, buoyancyForce * submersionFactor, 0);
					bodyInterface.AddForce(body.id, buoyancy);

					// Water drag: oppose body velocity
					Vec3 bodyVel = bodyInterface.GetLinearVelocity(body.id);
					Vec3 dragForce = -bodyVel * waterDragCoeff * submersionFactor;
					bodyInterface.AddForce(body.id, dragForce);

					// Water current force: push body in water flow direction
					Vec3 currentForce = Vec3(result.waterVelocity.x, 0, result.waterVelocity.y) * waterCurrentForceCoeff * submersionFactor;
					bodyInterface.AddForce(body.id, currentForce);
				}
			}

			waitingForResults = false;
		}
	}

	// Build new query for this frame: find dynamic bodies in water bounds
	std::vector<WaterSim::InteractionQuery> queries;
	trackedBodies.clear();

	for (UINT i = 0; i < (UINT)physics.dynamicBodies.size(); i++)
	{
		auto& body = physics.dynamicBodies[i];
		if (!physics.system->GetBodyInterface().IsActive(body.id))
			continue;

		Vector3 pos = body.entity->getPosition();

		// Check if body position is within water XZ bounds
		float relX = pos.x - bounds.center.x;
		float relZ = pos.z - bounds.center.z;

		if (relX >= 0 && relX <= bounds.size.x && relZ >= 0 && relZ <= bounds.size.y)
		{
			queries.push_back({ pos });
			trackedBodies.push_back({ i });

			if (queries.size() >= WaterSim::MaxInteractionQueries)
				break;
		}
	}

	if (!queries.empty())
	{
		water.submitInteractionQueries(queries);
		waitingForResults = true;
	}
}
