#pragma once

#include "Physics/JoltHeader.h"
#include "Jolt/Physics/Vehicle/VehicleConstraint.h"
#include "Scene/RenderWorld.h"
#include "ResourceUploadBatch.h"
#include "Objects/MovableBody.h"
#include "InputHandler.h"

class PhysicsManager;
struct GraphicsResources;

struct MotorcycleGraphics
{
	RenderEntity* chassis;
	RenderEntity* wheels[2];
};

class JoltMotorcycle : public MovableBody, public InputListener
{
public:

	JoltMotorcycle(PhysicsManager& physics);

	void initialize(RenderWorld&, GraphicsResources& resources, ResourceUploadBatch& batch);
	void setPositionOrientation(Vector3, Quaternion);

	void update(float delta);

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

	MotorcycleGraphics graphics;

	PhysicsManager& physics;

	static inline bool			sOverrideFrontSuspensionForcePoint = false;	///< If true, the front suspension force point is overridden
	static inline bool			sOverrideRearSuspensionForcePoint = false;	///< If true, the rear suspension force point is overridden
	static inline bool			sEnableLeanController = true;				///< If true, the lean controller is enabled
	static inline bool			sOverrideGravity = false;					///< If true, gravity is overridden to always oppose the ground normal

	JPH::Body* mMotorcycleBody;							///< The vehicle
	JPH::Ref<JPH::VehicleConstraint>		mVehicleConstraint;							///< The vehicle constraint
	//RMat44						mCameraPivot = RMat44::sIdentity();			///< The camera pivot, recorded before the physics update to align with the drawn world

	void updateInput(float delta);

	// Player input
	float						left = 0.0f;
	float						right = 0.0f;
	float						mForward = 0.0f;
	float						mPreviousForward = 1.0f;					///< Keeps track of last motorcycle direction so we know when to brake and when to accelerate
	float						mRight = 0.0f;								///< Keeps track of the current steering angle
	float						mBrake = 0.0f;

	Vector3 cachedPosition;
	Vector3 cachedDirection;
	Vector3 cachedVelocity;
};
