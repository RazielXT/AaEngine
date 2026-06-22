#pragma once

#include "Utils/Directx.h"
#include "Resources/Shader/ShaderDataBuffers.h"
#include "Resources/Shader/ShaderConstantsProvider.h"
#include <utility>
#include <array>

class VertexBufferModel;
struct InstanceGroup;

struct EntityGeometry;

using GeometryViewVariants = std::array<EntityGeometry*, RenderViewId_Count>;

template<RenderViewId... Views>
struct GeometryViews
{
	static constexpr RenderViewId usedGeometryViews[] = { Views... };
	static constexpr UINT count = sizeof...(Views);

	EntityGeometry viewGeometries[count];
	GeometryViewVariants variants;

	constexpr GeometryViews()
	{
		init(std::make_index_sequence<count>{});
	}

private:
	template<std::size_t... I>
	constexpr void init(std::index_sequence<I...>)
	{
		((variants[usedGeometryViews[I]] = &viewGeometries[I]), ...);
	}
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
	const GeometryViewVariants* viewVariants{};

	EntityGeometry& getGeometry(RenderViewId);

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