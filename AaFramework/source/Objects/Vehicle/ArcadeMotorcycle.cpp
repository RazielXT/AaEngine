#include "ArcadeMotorcycle.h"
#include "JoltPhysics/Jolt/Math/Math.h"
#include "JoltPhysics/Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Physics/PhysicsManager.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Constraints/HingeConstraint.h"

using namespace JPH;
#include "JoltPhysics/Samples/Layers.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"

ArcadeMotorcycle::ArcadeMotorcycle(PhysicsManager& p) : physics(p)
{

}

void ArcadeMotorcycle::initialize(RenderWorld& world, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	initializeGraphics(world, resources, batch);
	initializePhysics();
}

void ArcadeMotorcycle::setPositionOrientation(Vector3 position, Quaternion q)
{
	physics.system->GetBodyInterface().SetPositionAndRotation(motorcycleBody->GetID(), { position.x, position.y, position.z }, { q.x, q.y, q.z, q.w }, EActivation::DontActivate);
}

void ArcadeMotorcycle::initializeGraphics(RenderWorld& world, GraphicsResources& resources, ResourceUploadBatch& batch)
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
		auto model = resources.models.getCoreModel("wheel.mesh");
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

void ArcadeMotorcycle::initializePhysics()
{
	BodyInterface& bi = physics.system->GetBodyInterface();

	RefConst<Shape> shape =
		new OffsetCenterOfMassShape(
			new BoxShape(Vec3(0.35f, 0.25f, 0.9f)),
			Vec3(0, -0.4f, 0));

	BodyCreationSettings settings(
		shape,
		RVec3(0, 5, 0),
		Quat::sIdentity(),
		EMotionType::Dynamic,
		Layers::MOVING);

	settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
	settings.mMassPropertiesOverride.mMass = motorcycleMass;

	settings.mLinearDamping = 0.1f;
	settings.mAngularDamping = 1.0f;

	motorcycleBody = bi.CreateBody(settings);

	bi.AddBody(motorcycleBody->GetID(), EActivation::Activate);
}

void ArcadeMotorcycle::update(float delta)
{
	cachePhysicsState();

	updateGrounding(delta);

	applyStabilization(delta);

	applyDrive(delta);

	applySteering(delta);

	applyTraction(delta);

	applyDrag(delta);

	clampVelocities();

	updateGraphics(delta);
}

void ArcadeMotorcycle::cachePhysicsState()
{
	frame.position =
		motorcycleBody->GetPosition();

	frame.rotation =
		motorcycleBody->GetRotation();

	frame.forward =
		frame.rotation.RotateAxisZ();

	frame.right =
		frame.rotation.RotateAxisX();

	frame.up =
		frame.rotation.RotateAxisY();

	frame.linearVelocity =
		motorcycleBody->GetLinearVelocity();

	frame.angularVelocity =
		motorcycleBody->GetAngularVelocity();
}

void ArcadeMotorcycle::updateGrounding(float delta)
{
	updateSuspensionProbe(
		0,
		Vec3(0, 0, 0.9f),
		delta);

	updateSuspensionProbe(
		1,
		Vec3(0, 0, -0.9f),
		delta);

	frame.grounded =
		suspension[0].grounded ||
		suspension[1].grounded;
}

void ArcadeMotorcycle::applyStabilization(float delta)
{
	float steerInput =
		right - left;

	float speed =
		abs(frame.linearVelocity.Dot(frame.forward));

	float speedFactor =
		Clamp(speed / 20.0f, 0.0f, 1.0f);

	float targetLean =
		-steerInput *
		DegreesToRadians(maxLeanAngle) *
		speedFactor;

	Quat leanRot =
		Quat::sRotation(
			frame.forward,
			targetLean);

	Vec3 targetUp =
		leanRot.RotateAxisY();

	Vec3 axis =
		frame.up.Cross(targetUp);

	Vec3 pitchRollAV =
		frame.angularVelocity -
		frame.up *
		frame.angularVelocity.Dot(frame.up);

	Vec3 torque =
		axis * uprightStrength
		- pitchRollAV * uprightDamping;

	motorcycleBody->AddTorque(torque);
}

void ArcadeMotorcycle::clampVelocities()
{
	Vec3 av =
		motorcycleBody->GetAngularVelocity();

	float avLen = av.Length();

	if (avLen > angularClamp)
	{
		av = av / avLen * angularClamp;

		motorcycleBody->SetAngularVelocity(av);
	}

	Vec3 lv =
		motorcycleBody->GetLinearVelocity();

	float speed = lv.Length();

	if (speed > maxSpeed)
	{
		lv = lv.Normalized() * maxSpeed;

		motorcycleBody->SetLinearVelocity(lv);
	}
}

void ArcadeMotorcycle::applyDrive(float delta)
{
	if (!frame.grounded)
		return;

	float forwardSpeed =
		frame.linearVelocity.Dot(frame.forward);

	float speedFactor =
		1.0f -
		Clamp(forwardSpeed / maxSpeed, 0.0f, 1.0f);

	float driveForce =
		engineForce * speedFactor;

	RVec3 rearPos =
		frame.position +
		RVec3(frame.rotation * Vec3(0, 0, -0.9f));

	motorcycleBody->AddForce(
		frame.forward * mForward * driveForce,
		rearPos);

	RVec3 frontPos =
		frame.position +
		RVec3(frame.rotation * Vec3(0, 0, 0.9f));

	Vec3 velocity =
		motorcycleBody->GetLinearVelocity();

	Vec3 forwardVelocity =
		frame.forward * forwardSpeed;

	motorcycleBody->AddForce(
		-forwardVelocity * brakeForce * mBrake,
		frontPos);

	if (frame.grounded)
	{
		suspension[1].wheelAngularVelocity +=
			mForward * 45.0f * delta;
	}

	float slip =
		abs(mForward) *
		engineForce *
		0.01f;

	suspension[1].wheelAngularVelocity +=
		slip * delta;

	suspension[1].wheelAngularVelocity =
		std::lerp(
			suspension[1].wheelAngularVelocity,
			0.0f,
			delta * 2.5f);
}

void ArcadeMotorcycle::applySteering(float delta)
{
	float targetSteer =
		right - left;

	steerCurrent = std::lerp(
		steerCurrent,
		targetSteer,
		std::clamp(delta * 8.0f,0.f, 1.f));

	float steerInput =
		steerCurrent;

	if (abs(steerInput) < 0.001f)
		return;

	float speed =
		abs(frame.linearVelocity.Dot(frame.forward));

	float speedRatio =
		Clamp(speed / maxSteerSpeed, 0.0f, 1.0f);

	float lowSpeedFactor =
		Clamp(speed / 4.0f, 0.0f, 1.0f);

	float highSpeedFactor =
		1.0f - speedRatio;

	float steerFactor =
		lowSpeedFactor *
		(0.25f + highSpeedFactor * 0.75f);

	float steerAmount =
		steerTorque *
		steerInput *
		steerFactor;

	if (!frame.grounded)
	{
		steerAmount *= 0.15f;
	}

	motorcycleBody->AddTorque(
		frame.up * steerAmount);

	float yawVel =
		frame.angularVelocity.Dot(frame.up);

	motorcycleBody->AddTorque(
		-frame.up * yawVel * steerDamping);
}

void ArcadeMotorcycle::applyDrag(float delta)
{
	Vec3 velocity =
		motorcycleBody->GetLinearVelocity();

	float forwardSpeed =
		velocity.Dot(frame.forward);

	Vec3 forwardVelocity =
		frame.forward * forwardSpeed;

	motorcycleBody->AddForce(
		-forwardVelocity * forwardDrag);

	if (abs(mForward) < 0.01f)
	{
		motorcycleBody->AddForce(
			-forwardVelocity * coastDrag);
	}

	if (frame.grounded)
	{
		Vec3 lateralVelocity =
			frame.right *
			velocity.Dot(frame.right);

		motorcycleBody->AddForce(
			-lateralVelocity *
			lateralDrag);
	}

	if (frame.grounded)
	{
		float lowSpeedFriction = 30.0f;

		float speed =
			velocity.Length();

		if (speed < 3.0f &&
			abs(mForward) < 0.01f)
		{
			motorcycleBody->AddForce(
				-velocity *
				lowSpeedFriction);
		}
	}
}

void ArcadeMotorcycle::applyTraction(float delta)
{
	if (!frame.grounded)
		return;

	Vec3 groundNormal = getGroundNormal();

	float slopeDot =
		groundNormal.Dot(Vec3::sAxisY());

	float speed =
		frame.linearVelocity.Length();

	float speedFactor =
		Clamp(speed / 12.0f, 0.0f, 1.0f);

	float slopeTraction =
		Clamp(
			(slopeDot - 0.25f) / 0.75f,
			0.0f,
			1.0f);

	float tractionScale =
		std::lerp(
			slopeTraction,
			1.0f,
			speedFactor);


	Vec3 velocity =
		motorcycleBody->GetLinearVelocity();

	speed =
		velocity.Length();

	if (speed < tractionMinSpeed)
		return;

	float forwardSpeed =
		velocity.Dot(frame.forward);

	Vec3 desiredVelocity =
		frame.forward * forwardSpeed;

	Vec3 velocityError =
		desiredVelocity - velocity;

	motorcycleBody->AddForce(
		velocityError *
		tractionStrength *
		tractionScale *
		motorcycleMass);
}

void ArcadeMotorcycle::updateSuspensionProbe(int index, const Vec3& localPos, float delta)
{
	BodyInterface& bi = physics.system->GetBodyInterface();

	Quat rot = motorcycleBody->GetRotation();

	Vec3 up = rot.RotateAxisY();

	RVec3 worldPos =
		motorcycleBody->GetPosition() +
		RVec3(rot * localPos);

	RVec3 wheelPos =
		worldPos -
		up * (hoverHeight - 0.65f - suspension[index].compression);

	suspension[index].worldPosition =
		wheelPos;

	float groundDrivenSpin =
		frame.linearVelocity.Dot(frame.forward)
		/ wheelRadius;

	suspension[index].wheelAngularVelocity =
		std::lerp(
			suspension[index].wheelAngularVelocity,
			groundDrivenSpin,
			delta * 3.0f);

	suspension[index].wheelSpin +=
		suspension[index].wheelAngularVelocity *
		delta;

	Quat spinRot =
		Quat::sRotation(
			Vec3::sAxisX(),
			suspension[index].wheelSpin);

	float steerVisual = 0.0f;

	if (index == 0)
	{
		steerVisual =
			steerCurrent *
			DegreesToRadians(25.0f);
	}

	Quat steerRot =
		Quat::sRotation(
			Vec3::sAxisY(),
			steerVisual);

	suspension[index].wheelRotation =
		frame.rotation *
		steerRot *
		spinRot;

	float rayLength = hoverHeight * 2.0f;

	RRayCast ray(
		worldPos + up * 0.3f,
		-up * rayLength);

	RayCastResult hit;

	IgnoreSingleBodyFilter ignore(
		motorcycleBody->GetID());

	suspension[index].grounded = false;

	if (physics.system->GetNarrowPhaseQuery().CastRay(
		ray,
		hit,
		{}, {},
		ignore))
	{
		float distance = hit.mFraction * rayLength;

		float compression =
			hoverHeight - distance;

		compression = max(0.0f, compression);

		if (compression > 0.0f)
		{
			suspension[index].grounded = true;

			suspension[index].compression =
				compression;

			Vec3 velocity =
				motorcycleBody->GetPointVelocity(worldPos);

			float verticalVel =
				velocity.Dot(up);

			float force =
				compression * springStrength
				- verticalVel * springDamping;

			motorcycleBody->AddForce(
				up * force,
				worldPos);

			suspension[index].contactPoint =
				ray.GetPointOnRay(hit.mFraction);

			{
				BodyLockRead lock(
					physics.system->GetBodyLockInterface(),
					hit.mBodyID);

				if (lock.Succeeded())
				{
					const Body& body = lock.GetBody();

					RVec3 hitPos =
						ray.GetPointOnRay(hit.mFraction);

					suspension[index].contactNormal =
						body.GetWorldSpaceSurfaceNormal(
							hit.mSubShapeID2,
							hitPos);
				}
			}
		}
	}
}

Vec3 ArcadeMotorcycle::getGroundNormal() const
{
	Vec3 normal = Vec3::sZero();

	int count = 0;

	for (int i = 0; i < 2; ++i)
	{
		if (suspension[i].grounded)
		{
			normal += suspension[i].contactNormal;
			count++;
		}
	}

	if (count == 0)
		return Vec3::sAxisY();

	return normal.Normalized();
}

void ArcadeMotorcycle::updateGraphics(float)
{
	Vec3 pos = motorcycleBody->GetPosition();
	Quat rot = motorcycleBody->GetRotation();

	Quaternion q(
		rot.GetX(),
		rot.GetY(),
		rot.GetZ(),
		rot.GetW());

	auto p = Vector3(
		pos.GetX(),
		pos.GetY(),
		pos.GetZ());

	graphics.chassis->setPositionOrientation(
		p,
		q);

	cachedPosition = p;

	Vec3 forward =
		motorcycleBody->GetRotation().RotateAxisZ();

	cachedDirection = Vector3(forward.GetX(), forward.GetY(), forward.GetZ());

	for (int i = 0; i < 2; ++i)
	{
		auto& s = suspension[i];

		Vector3 pos(
			s.worldPosition.GetX(),
			s.worldPosition.GetY(),
			s.worldPosition.GetZ());


		Quat rot = s.wheelRotation;

		Quaternion q(
			rot.GetX(),
			rot.GetY(),
			rot.GetZ(),
			rot.GetW());

		graphics.wheels[i]->setPositionOrientation(
			pos,
			q);

		graphics.wheels[i]->setScale(
			Vector3(0.2f, 0.35f, 0.35f));
	}
}

/*
void ArcadeMotorcycle::updateGraphics(float deltaTime)
{
	Vec3 bodyPos =
		motorcycleBody->GetPosition();

	Quat bodyRot =
		motorcycleBody->GetRotation();

	Vector3 physicsPosition(
		bodyPos.GetX(),
		bodyPos.GetY(),
		bodyPos.GetZ());

	Quaternion physicsRotation(
		bodyRot.GetX(),
		bodyRot.GetY(),
		bodyRot.GetZ(),
		bodyRot.GetW());

	float positionSharpness = 12.0f;
	float rotationSharpness = 14.0f;

	float posAlpha =
		std::clamp(
			deltaTime * positionSharpness,
			0.0f,
			1.0f);

	float rotAlpha =
		std::clamp(
			deltaTime * rotationSharpness,
			0.0f,
			1.0f);

	visualPosition =
		Vector3::Lerp(
			visualPosition,
			physicsPosition,
			posAlpha);

	cachedPosition =
		visualPosition;

	visualRotation =
		Quaternion::Slerp(
			visualRotation,
			physicsRotation,
			rotAlpha);

	graphics.chassis->setPositionOrientation(
		visualPosition,
		visualRotation);

	Vec3 forward =
		motorcycleBody->GetRotation().RotateAxisZ();

	cachedDirection = Vector3::UnitZ * visualRotation;



	for (int i = 0; i < 2; ++i)
	{
		auto& s = suspension[i];

		Vector3 wheelPos(
			visualPosition.x + s.worldPosition.GetX(),
			visualPosition.y + s.worldPosition.GetY(),
			visualPosition.z + s.worldPosition.GetZ());

		Quat wheelRot =
			s.wheelRotation;

		Quaternion q(
			wheelRot.GetX(),
			wheelRot.GetY(),
			wheelRot.GetZ(),
			wheelRot.GetW());

		graphics.wheels[i]->setPositionOrientation(
			wheelPos,
			q);

		graphics.wheels[i]->setScale(
			Vector3(0.2f, 0.35f, 0.35f));
	}
}*/

bool ArcadeMotorcycle::keyPressed(int key)
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

bool ArcadeMotorcycle::keyReleased(int key)
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

Vector3 ArcadeMotorcycle::getPosition() const
{
	return cachedPosition;
}

Vector3 ArcadeMotorcycle::getDirection() const
{
	return cachedDirection;
}

Vector3 ArcadeMotorcycle::getVelocity() const
{
	return cachedVelocity;
}
