#include "FirstPersonCamera.h"
#include "Objects/MovableBody.h"
#include <algorithm>
#include "Physics/PhysicsManager.h"

FirstPersonCamera::FirstPersonCamera(Camera& camera, PhysicsManager& ph) : CameraHandler(camera), physics(ph)
{
}

void FirstPersonCamera::activate(TargetViewport& viewport)
{
	yaw = -camera.getYaw();
	pitch = camera.getPitch();

	CameraHandler::activate(viewport);
}

void FirstPersonCamera::setTarget(MovableBody* body)
{
	target = body;
}

Vector3 FirstPersonCamera::getForwardDirection() const
{
	// Horizontal forward only (no pitch component for movement)
	float cosYaw = cosf(yaw);
	float sinYaw = sinf(yaw);
	return Vector3(sinYaw, 0, cosYaw);
}

void FirstPersonCamera::update(float time)
{
	if (!target)
		return;

	// Build look direction from yaw and pitch
	float cosP = cosf(pitch);
	Vector3 lookDir(sinf(yaw) * cosP, sinf(pitch), cosf(yaw) * cosP);
	lookDir.Normalize();

	Vector3 bodyPos = target->getPosition();

	if (thirdPerson)
	{
		// Camera behind and above the player
		Vector3 forward = getForwardDirection();
		Vector3 right = Vector3::Up.Cross(forward);
		right.Normalize();

		Vector3 camPos = bodyPos
			+ Vector3::Up * thirdPersonOffset.y
			+ forward * thirdPersonOffset.z
			+ right * thirdPersonOffset.x;

		Vector3 target = bodyPos + Vector3::Up * eyeHeight + lookDir * 2.0f;

		auto finalCamPos = limitCameraPosition(physics, camPos, target);

		camera.setPosition(finalCamPos);
		camera.lookAt(target);
	}
	else
	{
		Vector3 eyePos = bodyPos;
		eyePos.y += eyeHeight;

		camera.setPosition(eyePos);
		camera.lookAt(eyePos + lookDir);
	}
}

bool FirstPersonCamera::mouseMoved(int x, int y)
{
	yaw += x * sensitivity;
	pitch -= y * sensitivity;
	pitch = std::clamp(pitch, -maxPitch, maxPitch);

	return true;
}

bool FirstPersonCamera::mousePressed(MouseButton)
{
	return true;
}

bool FirstPersonCamera::mouseReleased(MouseButton)
{
	return true;
}
