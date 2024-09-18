#pragma once

#include <d3d11.h>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include <string>
#include <DirectXCollision.h>

using namespace DirectX;

namespace VertexElementSemantic
{
    constexpr const char* POSITION = "POSITION";
    constexpr const char* NORMAL = "NORMAL";
    constexpr const char* COLOR = "COLOR";
    constexpr const char* TEXCOORD = "TEXCOORD";
    constexpr const char* BINORMAL = "BINORMAL";
    constexpr const char* TANGENT = "TANGENT";
    /// Blending weights
    constexpr const char* BLEND_WEIGHTS = "BLENDWEIGHT";
    /// Blending indices
    constexpr const char* BLEND_INDICES = "BLENDINDICES";
}

class AaRenderSystem;

class AaModel
{
public:

	AaModel(AaRenderSystem*);
	~AaModel();

	void addLayoutElement(unsigned short source, size_t offset, DXGI_FORMAT format, const char* semantic, unsigned short index);
	uint32_t getLayoutVertexSize(uint16_t) const;
	std::vector<D3D11_INPUT_ELEMENT_DESC> vertexLayout;

	void buildVertexBuffer(const void* data, size_t size);
	ID3D11Buffer* vertexBuffer_ = nullptr;
	uint32_t vertexCount = 0;

	void buildIndexBuffer(const uint16_t* data);
	ID3D11Buffer* indexBuffer_ = nullptr;
	uint32_t indexCount = 0;

	uint64_t getLayoutId();

	DirectX::BoundingBox bbox;
	void calculateBounds(const std::vector<float>& positions);

private:

	AaRenderSystem* renderSystem;
	uint64_t layoutHash = 0;
};
