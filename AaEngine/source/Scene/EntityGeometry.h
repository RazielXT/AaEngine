#pragma once

#include "Utils/Directx.h"
#include <vector>
#include "Resources/Shader/ShaderDataBuffers.h"

class VertexBufferModel;
struct InstanceGroup;

// Optional per-view override of the GPU buffers an entity draws with.
// Used by GPU-culled indirect geometry (grass/vegetation) to render different
// culled data per camera (main view, shadow cascades, ...).
struct GeometryViewVariant
{
	D3D12_GPU_VIRTUAL_ADDRESS customBuffer{};
	void* indirectSource{};
};

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

	// When set, selects custom buffer / indirect source per render view (e.g. shadow cascade).
	// View 0 is the main camera; falls back to default fields when null or out of range.
	const GeometryViewVariant* viewVariants{};
	UINT viewVariantCount{};

	D3D12_GPU_VIRTUAL_ADDRESS resolveCustomBuffer(UINT viewId) const
	{
		if (viewVariants && viewId < viewVariantCount)
			return viewVariants[viewId].customBuffer;
		return geometryCustomBuffer;
	}

	void* resolveIndirectSource(UINT viewId) const
	{
		if (viewVariants && viewId < viewVariantCount)
			return viewVariants[viewId].indirectSource;
		return source;
	}

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