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
	if (vertexBuffer && owner)
		vertexBuffer->Release();
	if (indexBuffer && owner)
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

void VertexBufferModel::addLayoutElement(DXGI_FORMAT format, const char* semantic, unsigned short index)
{
	addLayoutElement(0, getLayoutVertexSize(0), format, semantic, index);
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
		D3D12_INPUT_ELEMENT_DESC* desc = nullptr;
		for (auto& f : vertexLayout)
			if (f.SemanticName == VertexElementSemantic::POSITION)
			{
				desc = &f;
				break;
			}

		if (desc)
		{
			positions.resize(vertexCount);
			for (size_t i = 0; i < vertexCount; i++)
			{
				auto positionsPtr = (float*)(((const uint8_t*)vertices) + i * vertexSize + desc->AlignedByteOffset);

				if (desc->Format == DXGI_FORMAT_R32G32B32_FLOAT)
				{
					positions[i] = { positionsPtr[0], positionsPtr[1], positionsPtr[2] };
				}
				else if (desc->Format == DXGI_FORMAT_R32_FLOAT)
				{
					positions[i] = { 0, positionsPtr[0], 0 };
				}
			}
		}
	}
}

void VertexBufferModel::CreateVertexBuffer(ID3D12Resource* buffer, UINT vc, UINT vertexSize)
{
	vertexCount = vc;
	vertexBuffer = buffer;

	// Create the vertex buffer view
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = vertexCount * vertexSize;
	vertexBufferView.StrideInBytes = vertexSize;
}

void VertexBufferModel::CreateIndexBuffer(ID3D12Device* device, ResourceUploadBatch* memory, const uint16_t* data, size_t dataCount)
{
	indexCount = (uint32_t)dataCount;

	auto hr = CreateStaticBuffer(device, *memory,
		data, dataCount,
		D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);

	// Create the index buffer view
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = static_cast<UINT>(dataCount) * sizeof(uint16_t);
	indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	indexBuffer->SetName(L"IB");

	indices.resize(dataCount);
	for (size_t i = 0; i < dataCount; i++)
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

void VertexBufferModel::CreateIndexBuffer(ID3D12Resource* buffer, uint32_t dataCount)
{
	owner = false;
	indexCount = dataCount;
	indexBuffer = buffer;

	// Create the index buffer view
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = dataCount * sizeof(uint16_t);
	indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}

template<typename T>
std::vector<T> CreateGridAlternatingIndices(UINT width)
{
	std::vector<T> data;
	data.resize((width - 1) * (width - 1) * 6);

	for (UINT x = 0; x < width - 1; x++)
		for (UINT y = 0; y < width - 1; y++)
		{
			int baseIndex = x + y * width;
			int indexOffset = 6 * (x + y * (width - 1));

			bool flip = (x + y) % 2 == 0;

			if (flip)
			{
				// Triangle pattern: |/
				data[indexOffset + 0] = (T)baseIndex;
				data[indexOffset + 1] = (T)baseIndex + width + 1;
				data[indexOffset + 2] = (T)baseIndex + 1;

				data[indexOffset + 3] = (T)baseIndex;
				data[indexOffset + 4] = (T)baseIndex + width;
				data[indexOffset + 5] = (T)baseIndex + width + 1;
			}
			else
			{
				// Triangle pattern: |\,
				data[indexOffset + 0] = (T)baseIndex;
				data[indexOffset + 1] = (T)baseIndex + width;
				data[indexOffset + 2] = (T)baseIndex + 1;

				data[indexOffset + 3] = (T)baseIndex + 1;
				data[indexOffset + 4] = (T)baseIndex + width;
				data[indexOffset + 5] = (T)baseIndex + width + 1;
			}
		}
	return data;
}

template<typename T>
std::vector<T> CreateGridIndices(UINT width)
{
	std::vector<T> indices;
	indices.reserve((width - 1) * (width - 1) * 6);

	for (UINT y = 0; y < width - 1; ++y)
	{
		for (UINT x = 0; x < width - 1; ++x)
		{
			T v0 = (T)y * width + x;
			T v1 = (T)v0 + 1;
			T v2 = (T)v0 + width;
			T v3 = (T)v2 + 1;

			// First triangle
			indices.push_back(v0);
			indices.push_back(v2);
			indices.push_back(v1);

			// Second triangle
			indices.push_back(v1);
			indices.push_back(v2);
			indices.push_back(v3);
		}
	}
	return indices;
}

void VertexBufferModel::CreateIndexBufferGrid(ID3D12Device* device, ResourceUploadBatch* memory, UINT width, bool alternating)
{
	if (width * width <= 0xffff)
	{
		auto indices = alternating ? CreateGridAlternatingIndices<uint16_t>(width) : CreateGridIndices<uint16_t>(width);
		CreateIndexBuffer(device, memory, indices.data(), indices.size());
	}
	else
	{
		auto indices = alternating ? CreateGridAlternatingIndices<uint32_t>(width) : CreateGridIndices<uint32_t>(width);
		CreateIndexBuffer(device, memory, indices.data(), indices.size());
	}
}

void VertexBufferModel::CreateIndexBufferStrip(ID3D12Device* device, ResourceUploadBatch* memory, UINT width, UINT height)
{
	std::vector<uint32_t> indices;
	indices.reserve((width - 1) * (height - 1) * 6);

	// Set primitive topology to D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	for (UINT y = 0; y < height - 1; ++y)
	{
		// Process the row (strip)
		for (UINT x = 0; x < width; ++x) // Note: width, not width - 1
		{
			// Add indices for the current row (y) and the row below (y+1)
			indices.push_back(y * width + x);
			indices.push_back((y + 1) * width + x);
		}

		// Stitching (Degenerate Triangles)
		// If not the last row, insert two indices to jump to the next row's start
		if (y < height - 2)
		{
			// Duplicate the last two indices of the current row strip
			// This creates two zero-area (degenerate) triangles:
			// 1. (last, last, next_start) -> ignored
			// 2. (last, next_start, next_start) -> ignored
			indices.push_back((y + 1) * width + (width - 1)); // End of current strip
			indices.push_back((y + 1) * width + (width - 1)); // Repeat
		}
	}

	CreateIndexBuffer(device, memory, indices.data(), indices.size());
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
