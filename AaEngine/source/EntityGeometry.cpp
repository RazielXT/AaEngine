#include "EntityGeometry.h"
#include "VertexBufferModel.h"
#include "EntityInstancing.h"
#include "GrassArea.h"

bool EntityGeometry::usesInstancing() const
{
	return type == Type::Instancing;
}

void EntityGeometry::fromModel(const VertexBufferModel& model)
{
	vertexCount = model.vertexCount;
	indexCount = model.indexCount;
	vertexBufferView = model.vertexBufferView;
	indexBufferView = model.indexBufferView;
	layout = &model.vertexLayout;
	instanceCount = 1;
	type = Type::Model;
}

void EntityGeometry::fromInstancedModel(const VertexBufferModel& model, InstanceGroup& group)
{
	fromModel(model);
	instanceCount = group.count;
	geometryCustomBuffer = group.gpuBuffer->GetGPUVirtualAddress();
	type = Type::Instancing;
	source = &group;
}

void EntityGeometry::fromGrass(GrassArea& grass)
{
	geometryCustomBuffer = grass.gpuBuffer->GetGPUVirtualAddress();
	vertexCount = grass.vertexCount;
	instanceCount = 1;
	type = Type::Manual;
	source = &grass;
}
