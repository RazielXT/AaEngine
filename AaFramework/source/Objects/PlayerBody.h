#pragma once

#include "Objects/MovableBody.h"
#include "InputHandler.h"
#include "Physics/JoltHeader.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>

class PhysicsManager;
class Camera;
class RenderWorld;
class RenderEntity;
struct GraphicsResources;

class PlayerBody : public MovableBody, public InputListener
{
public:

	PlayerBody(PhysicsManager& physics);
	~PlayerBody();

	void initialize(Vector3 position);

	void update(float deltaTime, Camera& camera);

	bool keyPressed(int key) override;
	bool keyReleased(int key) override;
	bool mousePressed(MouseButton) override { return false; }
	bool mouseReleased(MouseButton) override { return false; }
	bool mouseMoved(int, int) override { return false; }
	bool mouseWheel(float) override { return false; }

	Vector3 getPosition() const override;
	Vector3 getDirection() const override;
	Vector3 getVelocity() const override;

	void createVisualBody(RenderWorld& world, GraphicsResources& resources);
	void destroyVisualBody(RenderWorld& world);
	bool hasVisualBody() const { return visualBody != nullptr; }

	float moveSpeed = 5.0f;
	float sprintMultiplier = 3.0f;
	float jumpSpeed = 6.0f;

private:

	PhysicsManager& physics;

	JPH::Ref<JPH::CharacterVirtual> character;

	bool w{}, s{}, a{}, d{}, sprint{}, jump{};
	Vector3 cachedPosition;
	Vector3 cachedDirection{ 0, 0, 1 };
	Vector3 cachedVelocity;

	RenderEntity* visualBody{};
};
