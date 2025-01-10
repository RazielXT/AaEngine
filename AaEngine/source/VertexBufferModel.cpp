#include "VertexBufferModel.h"
#include "RenderSystem.h"
#include <functional>
#include <BufferHelpers.h>
#include "MathUtils.h"

VertexBufferModel::VertexBufferModel()
{

}

VertexBufferModel::~VertexBufferModel()
{
	if (vertexBuffer)
		vertexBuffer->Release();
	if (indexBuffer)
		indexBuffer->Release();
}

void VertexBufferModel::addLayoutElement(unsigned short slot, UINT offset, DXGI_FORMAT format, const char* semantic, unsigned short index)
{
	D3D12_INPUT_ELEMENT_DESC desc{};
	desc.Format = format;
	desc.SemanticName = semantic;
	desc.AlignedByteOffset = offset;
	desc.SemanticIndex = index;
	desc.InputSlot = slot;

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
	}
}

uint32_t VertexBufferModel::getLayoutVertexSize(uint16_t slot) const
{
	uint32_t sz = 0;
	for (const auto& e : vertexLayout)
	{
		if (e.InputSlot == slot)
			sz += getTypeSize(e.Format);
	}
	return sz;
}

void VertexBufferModel::CreateVertexBuffer(ID3D12Device* device, ResourceUploadBatch* memory, const void* vertices, UINT vc, UINT vertexSize)
{
	vertexCount = vc;

	auto hr = CreateStaticBuffer(device, *memory,
		vertices,
		vertexCount,
		vertexSize,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &vertexBuffer);

	// Create the vertex buffer view
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = vertexCount * vertexSize;
	vertexBufferView.StrideInBytes = vertexSize;
	vertexBuffer->SetName(L"VB");

	{
		positions.resize(vertexCount);
		UINT positionOffset = 0;
		for (auto& f : vertexLayout)
			if (f.SemanticName == VertexElementSemantic::POSITION)
			{
				positionOffset = f.AlignedByteOffset;
				break;
			}

		for (size_t i = 0; i < vertexCount; i++)
		{
			auto positionsPtr = (float*)(((const uint8_t*)vertices) + i * vertexSize + positionOffset);
			positions[i] = { positionsPtr[0], positionsPtr[1], positionsPtr[2], 1 };
		}
	}
}

void VertexBufferModel::CreateIndexBuffer(ID3D12Device* device, ResourceUploadBatch* memory, const std::vector<uint16_t>& data)
{
	indexCount = (uint32_t)data.size();

	auto hr = CreateStaticBuffer(device, *memory,
		data,
		D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);

	// Create the index buffer view
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = static_cast<UINT>(data.size()) * sizeof(uint16_t);
	indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	indexBuffer->SetName(L"IB");

	indices.resize(data.size());
	for (size_t i = 0; i < data.size(); i++)
	{
		indices[i] = data[i];
	}
}

void VertexBufferModel::CreateIndexBuffer(ID3D12Device* device, ResourceUploadBatch* memory, const uint32_t* data, size_t dataCount)
{
	indexCount = (uint32_t)dataCount;

	auto hr = CreateStaticBuffer(device, *memory,
		data, dataCount,
		D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);

	// Create the index buffer view
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = static_cast<UINT>(dataCount) * sizeof(uint32_t);
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBuffer->SetName(L"IB");

	indices.resize(dataCount);
	for (size_t i = 0; i < dataCount; i++)
	{
		indices[i] = data[i];
	}
}

void VertexBufferModel::calculateBounds(const std::vector<float>& positionsBuffer)
{
	BoundingBoxVolume volume;

	for (size_t i = 0; i < positionsBuffer.size(); i += 3)
	{
		volume.add({ positionsBuffer[i], positionsBuffer[i + 1], positionsBuffer[i + 2] });
	}

	bbox = volume.createBbox();
}

void VertexBufferModel::calculateBounds()
{
	BoundingBoxVolume volume;

	for (size_t i = 0; i < positions.size(); i++)
	{
		volume.add(positions[i]);
	}

	bbox = volume.createBbox();
}

const char* VertexElementSemantic::GetConstName(const std::string& semantic)
{
	if (semantic == VertexElementSemantic::POSITION)
		return VertexElementSemantic::POSITION;
	if (semantic == VertexElementSemantic::SV_POSITION)
		return VertexElementSemantic::SV_POSITION;
	if (semantic == VertexElementSemantic::TEXCOORD)
		return VertexElementSemantic::TEXCOORD;
	if (semantic == VertexElementSemantic::COLOR)
		return VertexElementSemantic::COLOR;
	if (semantic == VertexElementSemantic::NORMAL)
		return VertexElementSemantic::NORMAL;
	if (semantic == VertexElementSemantic::TANGENT)
		return VertexElementSemantic::TANGENT;
	if (semantic == VertexElementSemantic::BINORMAL)
		return VertexElementSemantic::BINORMAL;
	if (semantic == VertexElementSemantic::BLEND_WEIGHTS)
		return VertexElementSemantic::BLEND_WEIGHTS;
	if (semantic == VertexElementSemantic::BLEND_INDICES)
		return VertexElementSemantic::BLEND_INDICES;

	return nullptr;
}
