#include "JoltMotorcycle.h"
#include "JoltPhysics/Jolt/Math/Math.h"
#include "JoltPhysics/Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "JoltPhysics/Jolt/Physics/Vehicle/WheeledVehicleController.h"
#include "JoltPhysics/Jolt/Physics/Vehicle/MotorcycleController.h"
#include "Physics/PhysicsManager.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"

using namespace JPH;
#include "JoltPhysics/Samples/Layers.h"

JoltMotorcycle::JoltMotorcycle(PhysicsManager& p) : physics(p)
{

}

void JoltMotorcycle::initialize(RenderWorld& world, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	initializeGraphics(world, resources, batch);
	initializePhysics();
}

void JoltMotorcycle::setPositionOrientation(Vector3 position, Quaternion q)
{
	physics.system->GetBodyInterface().SetPositionAndRotation(mMotorcycleBody->GetID(), { position.x, position.y, position.z }, { q.x, q.y, q.z, q.w }, EActivation::DontActivate);
}

void JoltMotorcycle::initializeGraphics(RenderWorld& world, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	{
		//auto model = resources.models.getCoreModel("centeredBox.mesh");
		auto model = resources.models.getCoreModel("headingBox.mesh");
		auto material = resources.materials.getMaterial("Red");

		auto ent = world.createEntity();
		ent->geometry.fromModel(*model);
		ent->material = material;

		graphics.chassis = ent;
		graphics.chassis->setScale(Vector3(0.4f, 0.6f, 0.8f));
	}

	{
		auto model = resources.models.getCoreModel("wheelY.mesh");
		auto material = resources.materials.getMaterial("Tire");

		for (int i = 0; i < 2; i++)
		{
			auto ent = world.createEntity();
			ent->geometry.fromModel(*model);
			ent->material = material;

			graphics.wheels[i] = ent;
		}
	}
}

void JoltMotorcycle::initializePhysics()
{
	// Loosely based on: https://www.whitedogbikes.com/whitedogblog/yamaha-xj-900-specs/
	const float back_wheel_radius = 0.31f;
	const float back_wheel_width = 0.05f;
	const float back_wheel_pos_z = -0.75f;
	const float back_suspension_min_length = 0.3f;
	const float back_suspension_max_length = 0.5f;
	const float back_suspension_freq = 2.0f;
	const float back_brake_torque = 250.0f;

	const float front_wheel_radius = 0.31f;
	const float front_wheel_width = 0.05f;
	const float front_wheel_pos_z = 0.75f;
	const float front_suspension_min_length = 0.3f;
	const float front_suspension_max_length = 0.5f;
	const float front_suspension_freq = 1.5f;
	const float front_brake_torque = 500.0f;

	const float half_vehicle_length = 0.4f;
	const float half_vehicle_width = 0.2f;
	const float half_vehicle_height = 0.3f;

	const float max_steering_angle = DegreesToRadians(30);

	// Angle of the front suspension
	const float caster_angle = DegreesToRadians(30);

	// Create vehicle body
	RVec3 position(0, 2, 0);
	RefConst<Shape> motorcycle_shape = OffsetCenterOfMassShapeSettings(Vec3(0, -half_vehicle_height, 0), new BoxShape(Vec3(half_vehicle_width, half_vehicle_height, half_vehicle_length))).Create().Get();
	BodyCreationSettings motorcycle_body_settings(motorcycle_shape, position, Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
	motorcycle_body_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
	motorcycle_body_settings.mMassPropertiesOverride.mMass = 240.0f;

	auto& bodyInterface = physics.system->GetBodyInterface();
	mMotorcycleBody = bodyInterface.CreateBody(motorcycle_body_settings);
	bodyInterface.AddBody(mMotorcycleBody->GetID(), EActivation::Activate);

	// Create vehicle constraint
	VehicleConstraintSettings vehicle;
	vehicle.mDrawConstraintSize = 0.1f;
	vehicle.mMaxPitchRollAngle = DegreesToRadians(60.0f);

	// Wheels
	WheelSettingsWV* front = new WheelSettingsWV;
	front->mPosition = Vec3(0.0f, -0.9f * half_vehicle_height, front_wheel_pos_z);
	front->mMaxSteerAngle = max_steering_angle;
	front->mSuspensionDirection = Vec3(0, -1, Tan(caster_angle)).Normalized();
	front->mSteeringAxis = -front->mSuspensionDirection;
	front->mRadius = front_wheel_radius;
	front->mWidth = front_wheel_width;
	front->mSuspensionMinLength = front_suspension_min_length;
	front->mSuspensionMaxLength = front_suspension_max_length;
	front->mSuspensionSpring.mFrequency = front_suspension_freq;
	front->mMaxBrakeTorque = front_brake_torque;

	WheelSettingsWV* back = new WheelSettingsWV;
	back->mPosition = Vec3(0.0f, -0.9f * half_vehicle_height, back_wheel_pos_z);
	back->mMaxSteerAngle = 0.0f;
	back->mRadius = back_wheel_radius;
	back->mWidth = back_wheel_width;
	back->mSuspensionMinLength = back_suspension_min_length;
	back->mSuspensionMaxLength = back_suspension_max_length;
	back->mSuspensionSpring.mFrequency = back_suspension_freq;
	back->mMaxBrakeTorque = back_brake_torque;

	if (sOverrideFrontSuspensionForcePoint)
	{
		front->mEnableSuspensionForcePoint = true;
		front->mSuspensionForcePoint = front->mPosition + front->mSuspensionDirection * front->mSuspensionMinLength;
	}

	if (sOverrideRearSuspensionForcePoint)
	{
		back->mEnableSuspensionForcePoint = true;
		back->mSuspensionForcePoint = back->mPosition + back->mSuspensionDirection * back->mSuspensionMinLength;
	}

	vehicle.mWheels = { front, back };

	MotorcycleControllerSettings* controller = new MotorcycleControllerSettings;
	controller->mEngine.mMaxTorque = 150.0f;
	controller->mEngine.mMinRPM = 1000.0f;
	controller->mEngine.mMaxRPM = 10000.0f;
	controller->mTransmission.mShiftDownRPM = 2000.0f;
	controller->mTransmission.mShiftUpRPM = 8000.0f;
	controller->mTransmission.mGearRatios = { 2.27f, 1.63f, 1.3f, 1.09f, 0.96f, 0.88f }; // From: https://www.blocklayer.com/rpm-gear-bikes
	controller->mTransmission.mReverseGearRatios = { -4.0f };
	controller->mTransmission.mClutchStrength = 2.0f;
	vehicle.mController = controller;

	// Differential (not really applicable to a motorcycle but we need one anyway to drive it)
	controller->mDifferentials.resize(1);
	controller->mDifferentials[0].mLeftWheel = -1;
	controller->mDifferentials[0].mRightWheel = 1;
	controller->mDifferentials[0].mDifferentialRatio = 1.93f * 40.0f / 16.0f; // Combining primary and final drive (back divided by front sprockets) from: https://www.blocklayer.com/rpm-gear-bikes

	mVehicleConstraint = new VehicleConstraint(*mMotorcycleBody, vehicle);
	mVehicleConstraint->SetVehicleCollisionTester(new VehicleCollisionTesterCastCylinder(Layers::MOVING, 1.0f)); // Use half wheel width as convex radius so we get a rounded cylinder

	physics.system->AddConstraint(mVehicleConstraint);
	physics.system->AddStepListener(mVehicleConstraint);
}

void JoltMotorcycle::update(float delta)
{
	updateInput(delta);

	// On user input, assure that the motorcycle is active
	if (mRight != 0.0f || mForward != 0.0f || mBrake != 0.0f)
		physics.system->GetBodyInterface().ActivateBody(mMotorcycleBody->GetID());

	// Pass the input on to the constraint
	MotorcycleController* controller = static_cast<MotorcycleController*>(mVehicleConstraint->GetController());
	controller->SetDriverInput(mForward, mRight, mBrake, false);
	controller->EnableLeanController(sEnableLeanController);

	Vec3 pos{};
	Quat quat{};
	physics.system->GetBodyInterface().GetPositionAndRotation(mMotorcycleBody->GetID(), pos, quat);
	Quaternion q{ quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW() };

	{
		graphics.chassis->setPositionOrientation(Vector3{ pos.GetX(), pos.GetY(), pos.GetZ() }, q);
	}

	for (uint w = 0; w < 2; ++w)
	{
		const WheelSettings* settings = mVehicleConstraint->GetWheels()[w]->GetSettings();
		RMat44 m = mVehicleConstraint->GetWheelWorldTransform(w, Vec3::sAxisY(), Vec3::sAxisX()); // The cylinder we draw is aligned with Y so we specify that as rotational axis
		//mDebugRenderer->DrawCylinder(wheel_transform, 0.5f * settings->mWidth, settings->mRadius, Color::sGreen);

		 auto matrix = DirectX::XMMATRIX(
				(float)m(0, 0), (float)m(0, 1), (float)m(0, 2), (float)m(0, 3),
				(float)m(1, 0), (float)m(1, 1), (float)m(1, 2), (float)m(1, 3),
				(float)m(2, 0), (float)m(2, 1), (float)m(2, 2), (float)m(2, 3),
				(float)m(3, 0), (float)m(3, 1), (float)m(3, 2), (float)m(3, 3)
			);

		 matrix = XMMatrixTranspose(matrix);

		 ObjectTransformation tr;
		 tr.fromWorldMatrix(matrix);

		 graphics.wheels[w]->setWorldMatrix(matrix);
		 graphics.wheels[w]->setScale(Vector3(0.35f, 0.2f, 0.35f));
		//Quaternion q{ quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW() };
		//graphics.wheels[w]->setPositionOrientation(Vector3{ pos.GetX(), pos.GetY(), pos.GetZ() }, q);
	}

	cachedPosition = { pos.GetX(), pos.GetY(), pos.GetZ() };
	cachedDirection = q * Vector3(0,0,1);

	auto vel = physics.system->GetBodyInterface().GetLinearVelocity(mMotorcycleBody->GetID());
	cachedVelocity = { vel.GetX(), vel.GetY(), vel.GetZ() };
}

bool JoltMotorcycle::keyPressed(int key)
{
	switch (key)
	{
	case 'Z':
		mBrake = 1.0f;
		break;
	case 'W':
		mForward = 1.0f;
		break;
	case 'S':
		mForward = -1.0f;
		break;
	case 'A':
		left = 1.0f;
		break;
	case 'D':
		right = 1.0f;
		break;
	default:
		return false;
	}

	return true;
}

bool JoltMotorcycle::keyReleased(int key)
{
	switch (key)
	{
	case 'Z':
		mBrake = 0.0f;
		break;
	case 'W':
		mForward = 0.0f;
		break;
	case 'S':
		mForward = 0.0f;
		break;
	case 'A':
		left = 0.0f;
		break;
	case 'D':
		right = 0.0f;
		break;
	default:
		return false;
	}

	return true;
}

void JoltMotorcycle::updateInput(float delta)
{
	// Check if we're reversing direction
	if (mPreviousForward * mForward < 0.0f)
	{
		// Get vehicle velocity in local space to the body of the vehicle
		float velocity = (mMotorcycleBody->GetRotation().Conjugated() * mMotorcycleBody->GetLinearVelocity()).GetZ();
		if ((mForward > 0.0f && velocity < -0.1f) || (mForward < 0.0f && velocity > 0.1f))
		{
			// Brake while we've not stopped yet
			mForward = 0.0f;
			mBrake = 1.0f;
		}
		else
		{
			// When we've come to a stop, accept the new direction
			mPreviousForward = mForward;
		}
	}

	// Steering
	float angle = left - right;

	const float steer_speed = 4.0f;
	if (angle > mRight)
		mRight = min(mRight + steer_speed * delta, angle);
	else if (angle < mRight)
		mRight = max(mRight - steer_speed * delta, angle);

	// When leaned, we don't want to use the brakes fully as we'll spin out
	if (mBrake > 0.0f)
	{
		Vec3 world_up = -physics.system->GetGravity().Normalized();
		Vec3 up = mMotorcycleBody->GetRotation() * mVehicleConstraint->GetLocalUp();
		Vec3 fwd = mMotorcycleBody->GetRotation() * mVehicleConstraint->GetLocalForward();
		float sin_lean_angle = abs(world_up.Cross(up).Dot(fwd));
		float brake_multiplier = Square(1.0f - sin_lean_angle);
		mBrake *= brake_multiplier;
	}
}

Vector3 JoltMotorcycle::getPosition() const
{
	return cachedPosition;
}

Vector3 JoltMotorcycle::getDirection() const
{
	return cachedDirection;
}

Vector3 JoltMotorcycle::getVelocity() const
{
	return cachedVelocity;
}
