#include "PlayerBody.h"
#include "Physics/PhysicsManager.h"
#include "FirstPersonCamera.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include "Scene/RenderWorld.h"

using namespace JPH;
#include "JoltPhysics/Samples/Layers.h"

PlayerBody::PlayerBody(PhysicsManager& p) : physics(p)
{
}

PlayerBody::~PlayerBody()
{
}

void PlayerBody::initialize(Vector3 position)
{
	CharacterVirtualSettings settings;
	settings.mShape = new CapsuleShape(0.5f * 1.4f, 0.3f); // half height * height, radius
	settings.mMaxSlopeAngle = DegreesToRadians(50.0f);
	settings.mMaxStrength = 100.0f;
	settings.mMass = 70.0f;
	settings.mPenetrationRecoverySpeed = 1.0f;
	settings.mPredictiveContactDistance = 0.1f;
	settings.mInnerBodyLayer = Layers::MOVING;

	character = new CharacterVirtual(&settings, RVec3(position.x, position.y, position.z), Quat::sIdentity(), 0, physics.system);

	cachedPosition = position;
}

void PlayerBody::update(float deltaTime, Camera& camera)
{
	if (!character)
		return;

	Vector3 forward = camera.getCameraDirection();
	Vector3 right = Vector3::Up.Cross(forward);
	right.Normalize();

	Vector3 moveDir{};
	if (w) moveDir += forward;
	if (s) moveDir -= forward;
	if (a) moveDir -= right;
	if (d) moveDir += right;

	if (moveDir != Vector3::Zero)
		moveDir.Normalize();

	float speed = moveSpeed;
	if (sprint)
		speed *= sprintMultiplier;

	Vec3 desiredVelocity(moveDir.x * speed, 0, moveDir.z * speed);

	// Apply gravity if not on ground, handle jump
	Vec3 currentVelocity = character->GetLinearVelocity();
	bool onGround = character->GetGroundState() == CharacterVirtual::EGroundState::OnGround;

	if (onGround)
	{
		if (jump)
		{
			desiredVelocity.SetY(jumpSpeed);
			jump = false;
		}
		else
		{
			desiredVelocity.SetY(0);
		}
	}
	else
	{
		desiredVelocity.SetY(currentVelocity.GetY() - 9.81f * deltaTime);
	}

	character->SetLinearVelocity(desiredVelocity);

	CharacterVirtual::ExtendedUpdateSettings updateSettings;

	character->ExtendedUpdate(deltaTime,
		-character->GetUp() * physics.system->GetGravity().Length(),
		updateSettings,
		physics.system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
		physics.system->GetDefaultLayerFilter(Layers::MOVING),
		{},
		{},
		physics.getTempAllocator());

	auto pos = character->GetPosition();
	cachedPosition = { (float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ() };

	cachedDirection = forward;

	auto vel = character->GetLinearVelocity();
	cachedVelocity = { vel.GetX(), vel.GetY(), vel.GetZ() };

	if (visualBody)
	{
		// cylinder.mesh: radius=1, height=1 -> scale to match capsule (radius=0.3, height~1.4)
		visualBody->setPosition(cachedPosition + Vector3(0, 0.7f, 0));
		visualBody->setScale(Vector3(0.3f, 1.4f, 0.3f));
	}
}

bool PlayerBody::keyPressed(int key)
{
	switch (key)
	{
	case 'W': w = true; break;
	case 'S': s = true; break;
	case 'A': a = true; break;
	case 'D': d = true; break;
	case VK_SHIFT: sprint = true; break;
	case VK_SPACE: jump = true; break;
	default: return false;
	}
	return true;
}

bool PlayerBody::keyReleased(int key)
{
	switch (key)
	{
	case 'W': w = false; break;
	case 'S': s = false; break;
	case 'A': a = false; break;
	case 'D': d = false; break;
	case VK_SHIFT: sprint = false; break;
	default: return false;
	}
	return true;
}

Vector3 PlayerBody::getPosition() const
{
	return cachedPosition;
}

Vector3 PlayerBody::getDirection() const
{
	return cachedDirection;
}

Vector3 PlayerBody::getVelocity() const
{
	return cachedVelocity;
}

void PlayerBody::createVisualBody(RenderWorld& world, GraphicsResources& resources)
{
	if (visualBody)
		return;

	auto model = resources.models.getCoreModel("cylinder.mesh");
	auto material = resources.materials.getMaterial("General");

	visualBody = world.createEntity();
	visualBody->geometry.fromModel(*model);
	visualBody->material = material;
}

void PlayerBody::destroyVisualBody(RenderWorld& world)
{
	if (!visualBody)
		return;

	world.removeEntity(visualBody);
	visualBody = nullptr;
}
