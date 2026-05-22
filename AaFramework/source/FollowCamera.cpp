#include "FollowCamera.h"
#include "Objects/MovableBody.h"
#include <algorithm>

FollowCamera::FollowCamera(Camera& camera, PhysicsManager& p) : CameraHandler(camera), physics(p)
{
}

void FollowCamera::activate(TargetViewport& viewport)
{
	initialized = false;
	smoothedTargetPosition = camera.getPosition();
	currentLookDirection = camera.getCameraDirection();

	CameraHandler::activate(viewport);
}

void FollowCamera::setTarget(MovableBody* body)
{
	target = body;
}

void FollowCamera::update(float time)
{
	if (!target)
		return;

	Vector3 targetPosition = target->getPosition();

	float targetSmoothSpeed = 8.f;
	float targetFactor = 1.0f - expf(-targetSmoothSpeed * time);

	smoothedTargetPosition =
		Vector3::Lerp(smoothedTargetPosition,
			target->getPosition(),
			targetFactor);

	targetPosition = smoothedTargetPosition;

	Vector3 objectForward = target->getDirection();
	Vector3 velocity = Vector3(0, 0, 0);// target->getVelocity();
	objectForward = -objectForward;
	velocity = -velocity;

	float speed = velocity.Length();

	float offsetByVelocity = max(0.8f, min(1.2f, speed * 0.04f));
	auto desiredOffset = offset * offsetByVelocity * 0.5f;

	// Blend look direction between objectForward and velocity direction based on speed
	Vector3 desiredLookDirection = objectForward;

	if (speed > minSpeedForVelocityBlend)
	{
		Vector3 velocityDirection = velocity;
		velocityDirection.Normalize();

		float t = min((speed - minSpeedForVelocityBlend) / (fullVelocityBlendSpeed - minSpeedForVelocityBlend), 1.0f);
		desiredLookDirection = Vector3::Lerp(objectForward, velocityDirection, t);
		desiredLookDirection.Normalize();
	}

	if (!initialized)
	{
		currentLookDirection = desiredLookDirection;
		initialized = true;

		// Snap position on first frame
		Vector3 up = Vector3::Up;
		Vector3 right = up.Cross(currentLookDirection);
		right.Normalize();

		Vector3 snapPosition = targetPosition
			- currentLookDirection * desiredOffset.z
			+ Vector3::Up * desiredOffset.y
			+ right * desiredOffset.x;

		camera.setPosition(snapPosition);
		camera.lookAt(targetPosition + Vector3::Up * (desiredOffset.y * 0.3f));
		return;
	}
	else
	{
		// Smooth rotation towards desired direction
		float rotFactor = 1.0f - expf(-rotationSmoothSpeed * time);
		currentLookDirection = Vector3::Lerp(currentLookDirection, desiredLookDirection, rotFactor);
		currentLookDirection.Normalize();
	}

	// Build orientation from look direction (Y-up)
	Vector3 up = Vector3::Up;
	Vector3 right = up.Cross(currentLookDirection);
	right.Normalize();
	up = currentLookDirection.Cross(right);
	up.Normalize();

	// Calculate desired camera position: behind and above the target in look-direction space
	Vector3 desiredPosition = targetPosition
		- currentLookDirection * desiredOffset.z   // behind
		+ Vector3::Up * desiredOffset.y            // above
		+ right * desiredOffset.x;                 // sideways

	// Smooth position
	float posFactor = 1.0f - expf(-positionSmoothSpeed * time);
	Vector3 currentPosition = camera.getPosition();
	Vector3 newPosition = Vector3::Lerp(currentPosition, desiredPosition, posFactor);

	Vector3 target = targetPosition + Vector3::Up * (desiredOffset.y * 0.3f);

	auto finalCamPos = limitCameraPosition(physics, newPosition, target);

	camera.setPosition(newPosition);
	camera.lookAt(target);
}
