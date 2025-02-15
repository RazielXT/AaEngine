#include "EntityGeometry.h"
#include "VertexBufferModel.h"
#include "EntityInstancing.h"
#include "GrassArea.h"

bool EntityGeometry::usesInstancing() const
{
	return type == Type::Instancing;
}

void EntityGeometry::fromModel(VertexBufferModel& model)
{
	vertexCount = model.vertexCount;
	indexCount = model.indexCount;
	vertexBufferView = model.vertexBufferView;
	indexBufferView = model.indexBufferView;
	layout = &model.vertexLayout;
	instanceCount = 1;
	source = &model;
	type = Type::Model;
}

void EntityGeometry::fromInstancedModel(VertexBufferModel& model, InstanceGroup& group)
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
	indexCount = grass.indexCount;
	instanceCount = grass.instanceCount;
	type = Type::Manual;
	source = &grass;

	indexBufferView.BufferLocation = grass.indexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = grass.indexCount * sizeof(uint32_t);
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

VertexBufferModel* EntityGeometry::getModel() const
{
	if (type == Type::Model)
		return (VertexBufferModel*)source;

	return nullptr;
}

void IndirectEntityGeometry::draw(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxCommands,
		commandBuffer.Get(),
		0,
		nullptr,
		0);
}
