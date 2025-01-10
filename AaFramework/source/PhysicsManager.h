#pragma once

#include <windows.h>
#include "JoltHeader.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "MathUtils.h"
#include <format>
#include "PhysicsRenderer.h"

class SceneEntity;
struct ObjectTransformation;

class PhysicsManager
{
public:

	PhysicsManager();
	~PhysicsManager();

	void init();

	JPH::BodyID createStaticBox(Vector3 extends, const ObjectTransformation& transformation, Vector3 offset);
	JPH::BodyID createDynamicBox(Vector3 extends, const ObjectTransformation& transformation, Vector3 offset, float mass);

	JPH::BodyID createStaticSphere(float radius, Vector3 position);
	JPH::BodyID createDynamicSphere(float radius, Vector3 position, Vector3 velocity);

	JPH::BodyID createConvexBody(const std::vector<Vector3>& points, const ObjectTransformation& transformation, float mass);

	JPH::BodyID createMeshBody(const std::vector<Vector3>& points, const std::vector<uint32_t>& indices, const ObjectTransformation& transformation);

	struct RegisteredEntity
	{
		SceneEntity* entity;
		JPH::BodyID id;
	};
	std::vector<RegisteredEntity> dynamicBodies;

	void update(float);

	JPH::PhysicsSystem* system{};

	void enableRenderer(RenderSystem&, GraphicsResources&);
	void disableRenderer();

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

	std::unique_ptr<PhysicsRenderer> renderer;
};