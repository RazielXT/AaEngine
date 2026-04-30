#include "Scene/EntityGeometry.h"
#include "Resources/Model/VertexBufferModel.h"
#include "Scene/EntityInstancing.h"

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
	fromInstancedModel(model, group.count, group.gpuBuffer->GetGPUVirtualAddress());
	source = &group;
}

void EntityGeometry::fromInstancedModel(VertexBufferModel& model, UINT count, D3D12_GPU_VIRTUAL_ADDRESS instancingBuffer)
{
	fromModel(model);
	instanceCount = count;
	geometryCustomBuffer = instancingBuffer;
	type = Type::Instancing;
}

void EntityGeometry::fromMeshInstancedModel(UINT count, D3D12_GPU_VIRTUAL_ADDRESS instancingBuffer)
{
	instanceCount = count;
	geometryCustomBuffer = instancingBuffer;
	type = Type::Mesh;
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
