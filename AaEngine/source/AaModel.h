#pragma once

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include <string>
#include <DirectXCollision.h>
#include "ResourceUploadBatch.h"

using namespace DirectX;

namespace VertexElementSemantic
{
	constexpr const char* POSITION = "POSITION";
	constexpr const char* SV_POSITION = "SV_POSITION";
	constexpr const char* NORMAL = "NORMAL";
	constexpr const char* COLOR = "COLOR";
	constexpr const char* TEXCOORD = "TEXCOORD";
	constexpr const char* BINORMAL = "BINORMAL";
	constexpr const char* TANGENT = "TANGENT";
	constexpr const char* BLEND_WEIGHTS = "BLENDWEIGHT";
	constexpr const char* BLEND_INDICES = "BLENDINDICES";

	const char* GetConstName(const std::string&);
}

class AaModel
{
public:

	AaModel();
	~AaModel();

	void addLayoutElement(unsigned short slot, UINT offset, DXGI_FORMAT format, const char* semantic, unsigned short index);
	uint32_t getLayoutVertexSize(uint16_t slot) const;
	std::vector<D3D12_INPUT_ELEMENT_DESC> vertexLayout;

	void CreateVertexBuffer(ID3D12Device* device, ResourceUploadBatch* memory, void* vertices, UINT vertexBufferSize, UINT vertexSize);
	ID3D12Resource* vertexBuffer{};
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	uint32_t vertexCount = 0;

	void CreateIndexBuffer(ID3D12Device* device, ResourceUploadBatch* memory, const std::vector<uint16_t>& data);
	ID3D12Resource* indexBuffer{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	uint32_t indexCount = 0;

	DirectX::BoundingBox bbox;
	void calculateBounds(const std::vector<float>& positions);

};
