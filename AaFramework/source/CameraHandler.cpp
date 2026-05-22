#include "CameraHandler.h"
#include "Physics/PhysicsManager.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "Utils/Logger.h"

CameraHandler::CameraHandler(Camera& camera) : camera(camera)
{
}

void CameraHandler::activate(TargetViewport& viewport)
{
	bind(viewport);
}

void CameraHandler::deactivate(TargetViewport& viewport)
{
	for (auto it = viewport.listeners.begin(); it != viewport.listeners.end(); it++)
	{
		if (*it == this)
		{
			viewport.listeners.erase(it);
			break;
		}
	}
}

void CameraHandler::bind(TargetViewport& viewport)
{
	target = &viewport;
	viewport.listeners.push_back(this);

	onViewportResize(target->getWidth(), target->getHeight());
}

Vector3 CameraHandler::limitCameraPosition(PhysicsManager& physics, Vector3 pos, Vector3 target)
{
	using namespace JPH;
	auto system = physics.system;

	// Cast a ray to find the terrain
	RVec3 origin(target.x, target.y, target.z);
	auto direction = pos - target;
	RRayCast ray{ origin, RVec3(direction.x, direction.y, direction.z) };
	RayCastResult hit;
	if (system->GetNarrowPhaseQuery().CastRay(ray, hit, SpecifiedBroadPhaseLayerFilter(BroadPhaseLayer(0)), SpecifiedObjectLayerFilter(4)) && hit.mFraction)
	{
		Logger::log("hit");
		return target + hit.mFraction * direction;
	}

	return pos;
}


void CameraHandler::onViewportResize(UINT width, UINT height)
{
	camera.setPerspectiveCamera(70, width / (float)height, 0.1f, 4000.f);
}
