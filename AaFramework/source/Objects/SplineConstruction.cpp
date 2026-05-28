#include "Objects/SplineConstruction.h"

#include "Physics/PhysicsManager.h"
#include "RenderCore/RenderSystem.h"
#include "Resources/GraphicsResources.h"
#include "Resources/Material/MaterialResources.h"
#include "Scene/RenderEntity.h"
#include "Scene/RenderWorld.h"

void SplineConstruction::regenerate(RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, PhysicsManager* physicsManager, DirectX::ResourceUploadBatch& uploadBatch, const SplineConstructionCreateParams& params)
{
	removeFromWorld(renderWorld, physicsManager);

	mesh = SplineSweepMeshGenerator::generate(spline, profile, sweepSettings);
	if (mesh.empty())
		return;

	SplineSweepMeshGenerator::createVertexBufferModel(model, renderSystem.core.device, &uploadBatch, mesh);

	if (params.createRenderEntity)
	{
		entity = renderWorld.createEntity();
		entity->geometry.fromModel(model);
		entity->material = resources.materials.getMaterial(params.materialName, uploadBatch);
		entity->setBoundingBox(model.bbox);
		entity->setTransformation(params.transformation, true);
	}

	if (params.createMeshPhysics && physicsManager)
		physicsBody = physicsManager->createMeshBody(model.positions, model.indices, params.transformation);
}

void SplineConstruction::removeFromWorld(RenderWorld& renderWorld, PhysicsManager* physicsManager)
{
	if (physicsBody && physicsManager)
		physicsManager->removeBody(*physicsBody);
	physicsBody.reset();

	if (entity)
		renderWorld.removeEntity(entity);
	entity = nullptr;
}