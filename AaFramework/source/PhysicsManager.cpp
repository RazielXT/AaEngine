#include "PhysicsManager.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include "Jolt/Physics/Collision/PhysicsMaterialSimple.h"
#include "Jolt/Core/Color.h"
#include "SceneEntity.h"
#include <thread>
#include <format>

using namespace JPH;
#include <Samples/Layers.h>
#include <debugapi.h>
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "RenderObject.h"
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/ObjectStream/ObjectStreamIn.h>

BPLayerInterfaceImpl	mBroadPhaseLayerInterface;									// The broadphase layer interface that maps object layers to broadphase layers
ObjectVsBroadPhaseLayerFilterImpl mObjectVsBroadPhaseLayerFilter;					// Class that filters object vs broadphase layers
ObjectLayerPairFilterImpl mObjectVsObjectLayerFilter;								// Class that filters object vs object layers

static Vec3 toVec3(const Vector3& v)
{
	return { v.x, v.y, v.z };
}
static Quat toQuat(const Quaternion& q)
{
	return { q.x, q.y, q.z, q.w };
}

PhysicsManager::PhysicsManager() : job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1)
{
}

PhysicsManager::~PhysicsManager()
{
	clear();
}

void PhysicsManager::init()
{
	clear();

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	static constexpr uint32_t cNumBodies = 10240;
	static constexpr uint32_t cNumBodyMutexes = 0; // Autodetect
	static constexpr uint32_t cMaxBodyPairs = 65536;
	static constexpr uint32_t cMaxContactConstraints = 20480;

	system = new JPH::PhysicsSystem();

	// Create physics system
	system->Init(cNumBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, mBroadPhaseLayerInterface, mObjectVsBroadPhaseLayerFilter, mObjectVsObjectLayerFilter);

	JPH::PhysicsSettings physicsSettings{};
	system->SetPhysicsSettings(physicsSettings);

	JPH::Vec3 gravity = JPH::Vec3(0, -9.81f, 0);
	system->SetGravity(gravity);

	// Optimize the broadphase to make the first update fast
	system->OptimizeBroadPhase();
}

void PhysicsManager::clear()
{
	if (system)
	{
		delete system;
		system = nullptr;
	}

	dynamicBodies.clear();
}

namespace
{
	float DefaultObjectFriction = 0.2f;
	float DefaultObjectRestitution = 0.3f;

	RefConst<Shape> CreateBoxShape(Vec3 extends, Vec3 offset, Vec3 scale)
	{
		RefConst<Shape> shape;
		shape = new BoxShape(extends);

		if (offset != Vec3::sReplicate(0.0f))
			shape = RotatedTranslatedShapeSettings(offset, Quat::sIdentity(), shape).Create().Get();

		if (scale != Vec3::sReplicate(1.0f))
			shape = new ScaledShape(shape, scale);

		return shape;
	}

	RefConst<Shape> CreateSphereShape(float radius)
	{
		RefConst<Shape> shape;
		shape = new SphereShape(radius);

		return shape;
	}
}

JPH::BodyID PhysicsManager::createStaticBox(Vector3 extends, const ObjectTransformation& t, Vector3 offset)
{
	BodyCreationSettings creation_settings(CreateBoxShape(toVec3(extends), toVec3(offset), toVec3(t.scale)), toVec3(t.position), toQuat(t.orientation), EMotionType::Static, Layers::NON_MOVING);
	creation_settings.mMotionQuality = EMotionQuality::Discrete;
	creation_settings.mFriction = DefaultObjectFriction;
	creation_settings.mRestitution = DefaultObjectRestitution;

	return system->GetBodyInterface().CreateAndAddBody(creation_settings, EActivation::DontActivate);
}

JPH::BodyID PhysicsManager::createDynamicBox(Vector3 extends, const ObjectTransformation& t, Vector3 offset, float mass)
{
	BodyCreationSettings creation_settings(CreateBoxShape(toVec3(extends), toVec3(offset), toVec3(t.scale)), toVec3(t.position), toQuat(t.orientation), EMotionType::Dynamic, Layers::MOVING);
	creation_settings.mMotionQuality = EMotionQuality::Discrete;
	creation_settings.mFriction = DefaultObjectFriction;
	creation_settings.mRestitution = DefaultObjectRestitution;

	return system->GetBodyInterface().CreateAndAddBody(creation_settings, EActivation::Activate);
}

JPH::BodyID PhysicsManager::createStaticSphere(float radius, Vector3 position)
{
	BodyCreationSettings creation_settings(CreateSphereShape(radius), toVec3(position), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
	creation_settings.mMotionQuality = EMotionQuality::Discrete;
	creation_settings.mFriction = DefaultObjectFriction;
	creation_settings.mRestitution = DefaultObjectRestitution;

	return system->GetBodyInterface().CreateAndAddBody(creation_settings, EActivation::Activate);
}

JPH::BodyID PhysicsManager::createDynamicSphere(float radius, Vector3 position, Vector3 velocity)
{
	BodyCreationSettings creation_settings(CreateSphereShape(radius), toVec3(position), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
	creation_settings.mMotionQuality = EMotionQuality::Discrete;
	creation_settings.mFriction = DefaultObjectFriction;
	creation_settings.mRestitution = DefaultObjectRestitution;
	creation_settings.mLinearVelocity = toVec3(velocity);

	return system->GetBodyInterface().CreateAndAddBody(creation_settings, EActivation::Activate);
}

JPH::BodyID PhysicsManager::createConvexBody(const std::vector<Vector3>& points, const ObjectTransformation& t, float mass)
{
	std::vector<Vec3> inputData(points.size());
	for (size_t i = 0; i < points.size(); i++)
	{
		auto& p = points[i];
		inputData.push_back({ p.x, p.y, p.z });
	}

	auto shape = ConvexHullShapeSettings(inputData.data(), (int)inputData.size()).Create().Get();

	if (t.scale != Vector3::One)
		shape = new ScaledShape(shape, toVec3(t.scale));

	BodyCreationSettings creation_settings(shape, toVec3(t.position), toQuat(t.orientation), EMotionType::Dynamic, Layers::MOVING);
	creation_settings.mMotionQuality = EMotionQuality::Discrete;
	creation_settings.mFriction = DefaultObjectFriction;
	creation_settings.mRestitution = DefaultObjectRestitution;

	if (mass == 0.0f)
	{
		creation_settings.mMotionType = EMotionType::Static;
		creation_settings.mObjectLayer = Layers::NON_MOVING;
	}

	return system->GetBodyInterface().CreateAndAddBody(creation_settings, EActivation::Activate);
}

static void createTriangles(const std::vector<Vector3>& points, const std::vector<uint32_t>& indices, TriangleList& triangles, PhysicsMaterialList& materials)
{
	uint32 material_index = 0;

	for (size_t i = 0; i < indices.size(); i += 3)
	{
		Float3 v1 = *(Float3*)&points[indices[i]];
		Float3 v2 = *(Float3*)&points[indices[i + 1]];
		Float3 v3 = *(Float3*)&points[indices[i + 2]];
		triangles.push_back(Triangle(v1, v2, v3, material_index));
	}

	materials.push_back(new PhysicsMaterialSimple("Material " + ConvertToString(material_index), JPH::Color::sGetDistinctColor(material_index)));
}

static void createTriangles(const std::vector<Vector3>& points, TriangleList& triangles, PhysicsMaterialList& materials)
{
	uint32 material_index = 0;

	for (size_t i = 0; i < points.size(); i += 3)
	{
		Float3 v1 = *(Float3*)&points[i];
		Float3 v2 = *(Float3*)&points[i + 1];
		Float3 v3 = *(Float3*)&points[i + 2];
		triangles.push_back(Triangle(v1, v2, v3, material_index));
	}

	materials.push_back(new PhysicsMaterialSimple("Material " + ConvertToString(material_index), JPH::Color::sGetDistinctColor(material_index)));
}

JPH::BodyID PhysicsManager::createMeshBody(const std::vector<Vector3>& points, const std::vector<uint32_t>& indices, const ObjectTransformation& t)
{
	TriangleList triangles;
	PhysicsMaterialList materials;

	if (!indices.empty())
		createTriangles(points, indices, triangles, materials);
	else
		createTriangles(points, triangles, materials);

	auto shape = MeshShapeSettings(triangles, std::move(materials)).Create().Get();

	if (t.scale != Vector3::One)
		shape = new ScaledShape(shape, toVec3(t.scale));

	return system->GetBodyInterface().CreateAndAddBody(BodyCreationSettings(shape, toVec3(t.position), toQuat(t.orientation), EMotionType::Static, Layers::NON_MOVING), EActivation::DontActivate);
}

void PhysicsManager::update(float deltaTime)
{
	Vec3 pos{};
	Quat quat{};

	for (auto& info  : dynamicBodies)
	{
		system->GetBodyInterface().GetPositionAndRotation(info.id, pos, quat);
		Quaternion q{ quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW() };
		info.entity->setPositionOrientation(Vector3{ pos.GetX(), pos.GetY(), pos.GetZ() }, q);
	}

	// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
	const int cCollisionSteps = 1;

	// Step the world
	system->Update(deltaTime, cCollisionSteps, &temp_allocator, &job_system);
}

void PhysicsManager::enableRenderer(RenderSystem& rs, GraphicsResources& r)
{
	if (!renderer)
		renderer = std::make_unique<PhysicsRenderer>(rs, r);
}

void PhysicsManager::disableRenderer()
{
	renderer.reset();
}

void PhysicsManager::drawDebugRender(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider* constants, const std::vector<DXGI_FORMAT>& targets, bool wireframe)
{
	if (renderer)
	{
		BodyManager::DrawSettings drawSettings;
		drawSettings.mDrawShapeWireframe = wireframe;

		CommandsMarker marker(commandList, "RenderPhysics");

		renderer->PrepareForRendering(commandList, constants, targets);
		system->DrawBodies(drawSettings, renderer.get());
	}
}

void PhysicsManager::test()
{
	Ref<PhysicsScene> scene;
	if (!ObjectStreamIn::sReadObject("../data/terrain2.bof", scene))
		return;
	for (BodyCreationSettings& body : scene->GetBodies())
		body.mObjectLayer = Layers::NON_MOVING;
	scene->FixInvalidScales();
	scene->CreateBodies(system);
}

PhysicsManager::Allocator::Allocator()
{
	JPH::RegisterDefaultAllocator();
}
