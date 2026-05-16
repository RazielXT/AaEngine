#pragma once

#include "Physics/JoltHeader.h"
#include "Scene/RenderWorld.h"
#include "ResourceUploadBatch.h"
#include "Objects/MovableBody.h"
#include "InputHandler.h"
#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Constraints/Constraint.h"
#include "Physics/PhysicsUpdater.h"

class PhysicsManager;
struct GraphicsResources;

struct Wheel
{
	JPH::Body* body;

	Vector3 visualPosition;
};

class ArcadeMotorcycle : public MovableBody, public InputListener, public PhysicsUpdater
{
public:

	ArcadeMotorcycle(PhysicsManager& physics);
	~ArcadeMotorcycle();

	void initialize(RenderWorld&, GraphicsResources& resources, ResourceUploadBatch& batch);
	void setPositionOrientation(Vector3, Quaternion);

	void update(float delta);
	void updatePhysics(float delta) override;

	bool keyPressed(int key) override;
	bool keyReleased(int key) override;
	bool mousePressed(MouseButton) override { return false; }
	bool mouseReleased(MouseButton) override { return false; }
	bool mouseMoved(int, int) override { return false; }
	bool mouseWheel(float) override { return false; }

	Vector3 getPosition() const override;
	Vector3 getDirection() const override;
	Vector3 getVelocity() const override;

private:

	void initializePhysics();
	void initializeGraphics(RenderWorld&, GraphicsResources& resources, ResourceUploadBatch& batch);

	struct MotorcycleGraphics
	{
		RenderEntity* chassis;
		RenderEntity* wheels[2];
	};
	MotorcycleGraphics graphics;

	PhysicsManager& physics;

	JPH::Body* motorcycleBody;							///< The vehicle

	struct SuspensionPoint
	{
		bool grounded = false;

		JPH::RVec3 contactPoint;

		JPH::Vec3 contactNormal;

		float compression = 0.0f;

		JPH::RVec3 worldPosition;

		JPH::Quat wheelRotation;

		float wheelAngularVelocity = 0.0f;

		float wheelSpin = 0.0f;
	};

	SuspensionPoint suspension[2];

	void updateSuspensionProbe(
		int index,
		const JPH::Vec3& localPos,
		float delta);

	JPH::Vec3 getGroundNormal() const;

	const float motorcycleMass = 250.0f;
	const float wheelRadius = 0.35f;

	float hoverHeight = 1.2f;
	float springStrength = 9000.0f;
	float springDamping = 1200.0f;

	float engineForce = 2500.0f;
	float brakeForce = 2000.0f;

	float maxSpeed = 25.0f;
	float forwardDrag = 4.0f;
	float lateralDrag = 1.0f;
	float coastDrag = 7.0f;

	Vector3 localFrontProbe = Vector3(0.0f, 0.0f, 0.9f);
	Vector3 localRearProbe = Vector3(0.0f, 0.0f, -0.9f);

	float uprightStrength = 220.0f;
	float uprightDamping = 35.0f;
	float angularClamp = 8.0f;

	float steerTorque = 420.0f;
	float steerDamping = 25.0f;
	float maxSteerSpeed = 35.0f;
	float steerCurrent = 0.0f;

	float tractionStrength = 3.0f;
	float tractionMinSpeed = 2.0f;

	float maxLeanAngle = 38.0f;
	float leanStrength = 7.0f;
	float leanReturnSpeed = 4.0f;

	struct PhysicsFrame
	{
		JPH::RVec3 position;

		JPH::Quat rotation;

		JPH::Vec3 forward;
		JPH::Vec3 right;
		JPH::Vec3 up;

		JPH::Vec3 linearVelocity;
		JPH::Vec3 angularVelocity;

		bool grounded = false;
	};

	PhysicsFrame frame;

	void updateGraphics(float delta);

	void cachePhysicsState();
	void updateGrounding(float delta);
	void applyStabilization(float delta);
	void applyDrive(float delta);
	void applySteering(float delta);
	void applyDrag(float delta);
	void applyTraction(float delta);
	void clampVelocities();

	// Player input
	float						left = 0.0f;
	float						right = 0.0f;
	float						mForward = 0.0f;
	float						mPreviousForward = 1.0f;					///< Keeps track of last motorcycle direction so we know when to brake and when to accelerate
	float						mRight = 0.0f;								///< Keeps track of the current steering angle
	float						mBrake = 0.0f;

	Vector3 cachedPosition;
	Vector3 cachedDirection = Vector3(0,0,1);
	Vector3 cachedVelocity;

	Vector3 visualPosition;
	Quaternion visualRotation;
	Vector3 visualForward;
};
