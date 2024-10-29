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
	geometryCustomBuffer = group.buffer;
	type = Type::Instancing;
}

void EntityGeometry::fromGrass(const GrassArea& grass)
{
	geometryCustomBuffer = grass.cbuffer;
	vertexCount = grass.getVertexCount();
	instanceCount = 1;
	type = Type::Manual;
}
