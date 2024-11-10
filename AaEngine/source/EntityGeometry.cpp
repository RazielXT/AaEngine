#include "EntityGeometry.h"
#include "AaModel.h"
#include "EntityInstancing.h"
#include "GrassArea.h"

bool EntityGeometry::usesInstancing() const
{
	return type == Type::Instancing;
}

void EntityGeometry::fromModel(const AaModel& model)
{
	vertexCount = model.vertexCount;
	indexCount = model.indexCount;
	vertexBufferView = model.vertexBufferView;
	indexBufferView = model.indexBufferView;
	layout = &model.vertexLayout;
	instanceCount = 1;
	type = Type::Model;
}

void EntityGeometry::fromInstancedModel(const AaModel& model, InstanceGroup& group)
{
	fromModel(model);
	instanceCount = group.count;
	geometryCustomBuffer = group.gpuBuffer->GetGPUVirtualAddress();
	type = Type::Instancing;
}

void EntityGeometry::fromGrass(GrassArea& grass)
{
	geometryCustomBuffer = grass.gpuBuffer->GetGPUVirtualAddress();
	vertexCount = grass.vertexCount;
	instanceCount = 1;
	type = Type::Manual;
}
