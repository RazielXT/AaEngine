#include "AaModel.h"
#include "AaRenderSystem.h"
#include <functional>
#include "Math.h"

AaModel::AaModel(AaRenderSystem* rs) : renderSystem(rs)
{

}

AaModel::~AaModel()
{
	if (indexBuffer_)
		indexBuffer_->Release();

	if (vertexBuffer_)
		vertexBuffer_->Release();
}

void AaModel::addLayoutElement(unsigned short source, size_t offset, DXGI_FORMAT format, const char* semantic, unsigned short index)
{
	D3D11_INPUT_ELEMENT_DESC desc{};
	desc.Format = format;
	desc.SemanticName = semantic;
	desc.AlignedByteOffset = offset;
	desc.SemanticIndex = index;
	desc.InputSlot = source;

	vertexLayout.push_back(desc);
}

static uint32_t getTypeSize(DXGI_FORMAT t)
{
	switch (t)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 16;
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 12;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return 8;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return 4;
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return 2;
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return 1;
		// Compressed format  
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		return 16;
		// Compressed format    
	case DXGI_FORMAT_R1_UNORM:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 8; // Compressed format    
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		return 4;               
		// These are compressed, but bit-size information is unclear.        
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
		return 4;
	case DXGI_FORMAT_UNKNOWN:
	default:
		return 0;
		//throw gcnew InvalidOperationException("Cannot determine format element size; invalid format specified.");
	}
}

uint32_t AaModel::getLayoutVertexSize(uint16_t source) const
{
	uint32_t sz = 0;
	for (const auto& e : vertexLayout)
	{
		if (e.InputSlot == source)
			sz += getTypeSize(e.Format);
	}
	return sz;
}

void AaModel::buildVertexBuffer(const void* data, size_t size)
{
	D3D11_BUFFER_DESC vertexDesc;
	ZeroMemory(&vertexDesc, sizeof(vertexDesc));
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.ByteWidth = size;
	D3D11_SUBRESOURCE_DATA resourceData;
	ZeroMemory(&resourceData, sizeof(resourceData));
	resourceData.pSysMem = data;

	HRESULT d3dResult = renderSystem->getDevice()->CreateBuffer(&vertexDesc, &resourceData, &vertexBuffer_);

	if (FAILED(d3dResult))
	{
		//AaLogger::logErrorD3D("Failed to create vertex buffer", d3dResult);
		return;
	}
}

void AaModel::buildIndexBuffer(const uint16_t* data)
{
	D3D11_BUFFER_DESC indexDesc;
	ZeroMemory(&indexDesc, sizeof(indexDesc));
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexDesc.ByteWidth = sizeof(uint16_t) * indexCount;
	indexDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA resourceData;
	ZeroMemory(&resourceData, sizeof(resourceData));
	resourceData.pSysMem = data;
	HRESULT d3dResult = renderSystem->getDevice()->CreateBuffer(&indexDesc, &resourceData, &indexBuffer_);

	if (FAILED(d3dResult))
	{
		//AaLogger::logErrorD3D("Failed to create index buffer", d3dResult);
	}
}

uint64_t AaModel::getLayoutId()
{
	if (!layoutHash)
	{
		for (auto& l : vertexLayout)
		{
			layoutHash ^= std::hash<int>{}(l.Format);
			layoutHash ^= std::hash<const void*>{}(l.SemanticName);
			layoutHash ^= std::hash<int>{}(l.Format);
		}
	}

	return layoutHash;
}

void AaModel::calculateBounds(const std::vector<float>& positions)
{
	if (!positions.empty())
	{
		Vector3 minBounds(positions[0], positions[1], positions[2]);
		Vector3 maxBounds = minBounds;

		for (size_t i = 0; i < positions.size(); i += 3)
		{
			minBounds.x = min(minBounds.x, positions[i]);
			minBounds.y = min(minBounds.y, positions[i + 1]);
			minBounds.z = min(minBounds.z, positions[i + 2]);

			maxBounds.x = max(maxBounds.x, positions[i]);
			maxBounds.y = max(maxBounds.y, positions[i + 1]);
			maxBounds.z = max(maxBounds.z, positions[i + 2]);
		}

		bbox.Center = (maxBounds + minBounds) / 2;
		bbox.Extents = (maxBounds - minBounds) / 2;
	}

}
