#pragma once

#include "GraphicsMemory.h"
#include <memory>
#include <string>
#include <map>
#include "Utils/Directx.h"

class RenderSystem;

struct CbufferData
{
	DirectX::DX12::GraphicsResource data[FrameCount];
};

struct CbufferView
{
	CbufferView() = default;
	CbufferView(CbufferData&);

	DirectX::DX12::GraphicsResource* data[FrameCount];
};

struct StructuredBufferData
{
	ComPtr<ID3D12Resource> resource;
	D3D12_GPU_VIRTUAL_ADDRESS addr;
};

struct StructuredBufferView
{
	StructuredBufferData* data;
};

class ShaderDataBuffers
{
public:

	ShaderDataBuffers(ID3D12Device& device);
	~ShaderDataBuffers();

	static ShaderDataBuffers& get();

	CbufferData CreateCbufferResource(UINT size);
	CbufferView CreateCbufferResource(UINT size, std::string name);
	CbufferView GetCbufferResource(std::string name);

	ComPtr<ID3D12Resource> CreateUploadStructuredBuffer(const void* data, UINT dataSize, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ);
	ComPtr<ID3D12Resource> CreateUploadStructuredBuffer(const void* data, UINT dataSize, std::string name, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ);
	ComPtr<ID3D12Resource> CreateStructuredBuffer(UINT dataSize, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> CreateStructuredBuffer(UINT dataSize, std::string name, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT);
	StructuredBufferView GetStructuredBufferResource(std::string name);

private:

	std::unique_ptr<DirectX::DX12::GraphicsMemory> graphicsMemory;
	ID3D12Device& device;

	std::map<std::string, CbufferData> cbuffers;
	std::map<std::string, StructuredBufferData> strbuffers;
};