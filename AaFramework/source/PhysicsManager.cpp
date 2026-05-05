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
#include "Scene/RenderEntity.h"
#include "Scene/RenderObject.h"
#include <thread>
#include <format>

using namespace JPH;
#include <Samples/Layers.h>
#include <debugapi.h>
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
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
	accumulator = 0.0f;
}

namespace
{
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

static void applyBodyParams(BodyCreationSettings& s, const BodyParams& params)
{
	s.mFriction = params.friction;
	s.mRestitution = params.restitution;
	s.mLinearDamping = params.linearDamping;
	s.mAngularDamping = params.angularDamping;
	s.mLinearVelocity = toVec3(params.velocity);

	if (params.mass > 0)
	{
		s.mMotionType = EMotionType::Dynamic;
		s.mObjectLayer = Layers::MOVING;
		s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
		s.mMassPropertiesOverride.mMass = params.mass;
	}
	else
	{
		s.mMotionType = EMotionType::Static;
		s.mObjectLayer = Layers::NON_MOVING;
	}
}

JPH::BodyID PhysicsManager::createBox(Vector3 extends, const ObjectTransformation& t, Vector3 offset, const BodyParams& params)
{
	BodyCreationSettings creation_settings(CreateBoxShape(toVec3(extends), toVec3(offset), toVec3(t.scale)), toVec3(t.position), toQuat(t.orientation), EMotionType::Static, Layers::NON_MOVING);
	applyBodyParams(creation_settings, params);

	bool isDynamic = params.mass > 0;
	return system->GetBodyInterface().CreateAndAddBody(creation_settings, isDynamic ? EActivation::Activate : EActivation::DontActivate);
}

JPH::BodyID PhysicsManager::createSphere(float radius, Vector3 position, const BodyParams& params)
{
	BodyCreationSettings creation_settings(CreateSphereShape(radius), toVec3(position), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
	applyBodyParams(creation_settings, params);

	bool isDynamic = params.mass > 0;
	return system->GetBodyInterface().CreateAndAddBody(creation_settings, isDynamic ? EActivation::Activate : EActivation::DontActivate);
}

JPH::BodyID PhysicsManager::createConvexBody(const std::vector<Vector3>& points, const ObjectTransformation& t, const BodyParams& params)
{
	std::vector<Vec3> inputData;
	inputData.reserve(points.size());
	for (size_t i = 0; i < points.size(); i++)
	{
		auto& p = points[i];
		inputData.push_back({ p.x, p.y, p.z });
	}

	auto shape = ConvexHullShapeSettings(inputData.data(), (int)inputData.size()).Create().Get();

	if (t.scale != Vector3::One)
		shape = new ScaledShape(shape, toVec3(t.scale));

	BodyCreationSettings creation_settings(shape, toVec3(t.position), toQuat(t.orientation), EMotionType::Static, Layers::NON_MOVING);
	applyBodyParams(creation_settings, params);

	bool isDynamic = params.mass > 0;
	return system->GetBodyInterface().CreateAndAddBody(creation_settings, isDynamic ? EActivation::Activate : EActivation::DontActivate);
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
	if (!enableUpdating)
		return;

	accumulator += deltaTime;

	if (accumulator < fixedTimeStep)
		return;

	constexpr int maxSteps = 4;
	int steps = 0;
	while (accumulator >= fixedTimeStep && steps < maxSteps)
	{
		system->Update(fixedTimeStep, 1, &temp_allocator, &job_system);
		accumulator -= fixedTimeStep;
		steps++;
	}

	if (steps == maxSteps)
		accumulator = 0.0f;

	Vec3 pos{};
	Quat quat{};

	for (auto& info : dynamicBodies)
	{
		if (system->GetBodyInterface().IsActive(info.id))
		{
			system->GetBodyInterface().GetPositionAndRotation(info.id, pos, quat);
			Quaternion q{ quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW() };
			info.entity->setPositionOrientation(Vector3{ pos.GetX(), pos.GetY(), pos.GetZ() }, q);
		}
	}
}

void PhysicsManager::enableUpdate(bool enable)
{
	enableUpdating = enable;
}

void PhysicsManager::enableRenderer(RenderSystem& rs, GraphicsResources& r)
{
	if (!renderer)
		renderer = std::make_unique<PhysicsRenderer>(rs, r);
}

void PhysicsManager::disableRenderer(RenderSystem& rs)
{
	if (renderer)
	{
		rs.core.WaitForAllFrames();
		renderer.reset();
	}
}

void PhysicsManager::drawDebugRender(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider* constants, const std::vector<DXGI_FORMAT>& targets, bool wireframe)
{
	if (renderer)
	{
		BodyManager::DrawSettings drawSettings;
		drawSettings.mDrawShapeWireframe = wireframe;

		CommandsMarker marker(commandList, "RenderPhysics", PixColor::Debug);

		if (renderer->PrepareForRendering(commandList, constants, targets))
			system->DrawBodies(drawSettings, renderer.get());
	}
}

void PhysicsManager::test()
{
	Ref<PhysicsScene> scene;
	if (!ObjectStreamIn::sReadObject("../dependencies/JoltPhysics/Assets/terrain2.bof", scene))
		return;
	for (BodyCreationSettings& body : scene->GetBodies())
		body.mObjectLayer = Layers::NON_MOVING;
	scene->FixInvalidScales();
	scene->CreateBodies(system);
}

JPH::BodyID PhysicsManager::createHeightField(const float* heights, uint32_t resolution, Vector3 offset, Vector3 scale)
{
	HeightFieldShapeSettings settings(heights, Vec3(0, 0, 0), Vec3(scale.x, scale.y, scale.z), resolution);

	auto shapeResult = settings.Create();
	if (shapeResult.HasError())
		return JPH::BodyID();

	BodyCreationSettings creation_settings(shapeResult.Get(), toVec3(offset), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
	creation_settings.mFriction = 0.8f;
	creation_settings.mRestitution = 0.3f;

	return system->GetBodyInterface().CreateAndAddBody(creation_settings, EActivation::DontActivate);
}

void PhysicsManager::removeBody(JPH::BodyID id)
{
	if (!id.IsInvalid())
	{
		system->GetBodyInterface().RemoveBody(id);
		system->GetBodyInterface().DestroyBody(id);
	}
}

PhysicsManager::Allocator::Allocator()
{
	JPH::RegisterDefaultAllocator();
}
