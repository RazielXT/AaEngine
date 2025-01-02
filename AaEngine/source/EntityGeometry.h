#pragma once

#include "Directx.h"
#include <vector>
#include "ShaderDataBuffers.h"

class VertexBufferModel;
struct InstanceGroup;
struct GrassArea;

struct EntityGeometry
{
	enum Type
	{
		Model,
		Instancing,
		Manual,
	}
	type{};

	UINT vertexCount{};
	UINT indexCount{};
	UINT instanceCount{};
	const std::vector<D3D12_INPUT_ELEMENT_DESC>* layout{};

	bool usesInstancing() const;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	D3D12_GPU_VIRTUAL_ADDRESS geometryCustomBuffer{};

	void* source{};

	void fromModel(const VertexBufferModel& model);
	void fromInstancedModel(const VertexBufferModel& model, InstanceGroup& group);
	void fromGrass(GrassArea& grass);
};
