#pragma once

#include "CameraHandler.h"

class MovableBody;

class FollowCamera : public CameraHandler
{
public:

	FollowCamera(Camera& camera, PhysicsManager& physics);

	void update(float time) override;

	void activate() override;

	void setTarget(MovableBody* body);

	// Camera offset relative to target orientation (local space: x=right, y=up, z=back)
	Vector3 offset{ 0, 3.f, -8.f };

	float positionSmoothSpeed = 5.0f;
	float rotationSmoothSpeed = 3.0f;

	// Below this speed, camera looks in objectForward direction only
	float minSpeedForVelocityBlend = 5.0f;
	// Above this speed, camera looks fully in velocity direction
	float fullVelocityBlendSpeed = 20.0f;

private:

	PhysicsManager& physics;

	MovableBody* target{};
	Vector3 smoothedTargetPosition;
	Vector3 currentLookDirection{ 0, 0, 1 };
	bool initialized = false;
};
