#pragma once

#include <windows.h>
#include "JoltHeader.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "Utils/MathUtils.h"
#include <format>
#include "PhysicsRenderer.h"

class SceneEntity;
struct ObjectTransformation;

struct BodyParams
{
	float friction = 0.5f;
	float restitution = 0.3f;
	float linearDamping = 0.1f;
	float angularDamping = 0.2f;
	float mass = 0;
	Vector3 velocity = {};
};

class PhysicsManager
{
public:

	PhysicsManager();
	~PhysicsManager();

	void init();

	JPH::BodyID createBox(Vector3 extends, const ObjectTransformation& transformation, Vector3 offset, const BodyParams& params = {});

	JPH::BodyID createSphere(float radius, Vector3 position, const BodyParams& params = {});

	JPH::BodyID createConvexBody(const std::vector<Vector3>& points, const ObjectTransformation& transformation, const BodyParams& params = {});

	JPH::BodyID createMeshBody(const std::vector<Vector3>& points, const std::vector<uint32_t>& indices, const ObjectTransformation& transformation);

	JPH::BodyID createHeightField(const float* heights, uint32_t resolution, Vector3 offset, Vector3 scale);

	void removeBody(JPH::BodyID id);

	struct RegisteredEntity
	{
		SceneEntity* entity;
		JPH::BodyID id;
	};
	std::vector<RegisteredEntity> dynamicBodies;

	void update(float);

	void enableUpdate(bool);

	JPH::PhysicsSystem* system{};

	void enableRenderer(RenderSystem&, GraphicsResources&);
	void disableRenderer(RenderSystem& rs);

	void drawDebugRender(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider* constants, const std::vector<DXGI_FORMAT>& targets, bool wireframe);

	void test();

private:

	struct Allocator
	{
		Allocator();
	}
	allocator;


	JPH::TempAllocatorMalloc temp_allocator;

	// We need a job system that will execute physics jobs on multiple threads. Typically
	// you would implement the JobSystem interface yourself and let Jolt Physics run on top
	// of your own job scheduler. JobSystemThreadPool is an example implementation.
	JPH::JobSystemThreadPool job_system;

	void clear();

	float fixedTimeStep = 1.0f / 60.0f;
	float accumulator = 0.0f;

	bool enableUpdating = true;

	std::unique_ptr<PhysicsRenderer> renderer;
};