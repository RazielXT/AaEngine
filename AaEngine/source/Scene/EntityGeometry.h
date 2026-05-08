#pragma once

#include "Utils/Directx.h"
#include <vector>
#include "Resources/Shader/ShaderDataBuffers.h"

class VertexBufferModel;
struct InstanceGroup;
struct EntityGeometry
{
	uint16_t topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	enum Type : uint16_t
	{
		Model,
		Instancing,
		Manual,
		Indirect,
		Mesh,
	}
	type{};

	UINT vertexCount{};
	UINT indexCount{};
	UINT instanceCount{};
	const std::vector<D3D12_INPUT_ELEMENT_DESC>* layout{};

	bool usesInstancing() const;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	D3D12_GPU_VIRTUAL_ADDRESS geometryCustomBuffer{};
	D3D12_GPU_VIRTUAL_ADDRESS geometryRedirectBuffer{};

	void* source{};

	void fromModel(VertexBufferModel& model);
	void fromInstancedModel(VertexBufferModel& model, InstanceGroup& group);
	void fromInstancedModel(VertexBufferModel& model, UINT count, D3D12_GPU_VIRTUAL_ADDRESS instancingBuffer);
	void fromMeshInstancedModel(UINT count, D3D12_GPU_VIRTUAL_ADDRESS instancingBuffer);

	VertexBufferModel* getModel() const;
};

class IndirectEntityGeometry
{
public:

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> commandBuffer;

	UINT maxCommands{};
	UINT commandBufferOffset{};

	void draw(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
};