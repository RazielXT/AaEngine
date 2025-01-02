#pragma once

#include "GraphicsMemory.h"
#include <memory>
#include <string>
#include <map>
#include "Directx.h"

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
	ComPtr<ID3D12Resource> CreateStructuredBuffer(UINT dataSize, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT);

private:

	std::unique_ptr<DirectX::DX12::GraphicsMemory> graphicsMemory;
	ID3D12Device& device;

	std::map<std::string, CbufferData> cbuffers;
};