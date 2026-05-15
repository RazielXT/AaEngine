#pragma once

#include "CameraHandler.h"
#include "InputHandler.h"

class MovableBody;
class PhysicsManager;

class FirstPersonCamera : public CameraHandler
{
public:

	FirstPersonCamera(Camera& camera, PhysicsManager& physics);

	void update(float time) override;

	void activate() override;

	void setTarget(MovableBody* body);

	bool mouseMoved(int x, int y) override;
	bool mousePressed(MouseButton button) override;
	bool mouseReleased(MouseButton button) override;

	float getYaw() const { return yaw; }
	float getPitch() const { return pitch; }
	Vector3 getForwardDirection() const;

	// Eye height offset above the body position
	float eyeHeight = 1.7f;

	// Third person offset (0 = first person, negative = behind)
	bool thirdPerson = false;
	Vector3 thirdPersonOffset{ 0, 2.5f, -4.f };

private:

	PhysicsManager& physics;
	MovableBody* target{};

	float yaw{};
	float pitch{};
	float sensitivity = 0.003f;

	static constexpr float maxPitch = 1.4f; // ~80 degrees
};
