#pragma once

#include "GraphicsMemory.h"
#include <memory>
#include <string>
#include <map>
#include "Directx.h"

class AaRenderSystem;

struct CbufferData
{
	DirectX::DX12::GraphicsResource data[2];
};

struct CbufferView
{
	CbufferView() = default;
	CbufferView(CbufferData&);

	DirectX::DX12::GraphicsResource* data[2];
};

class ShaderConstantBuffers
{
public:

	ShaderConstantBuffers(AaRenderSystem* rs);
	~ShaderConstantBuffers();

	static ShaderConstantBuffers& get();

	CbufferData CreateCbufferResource(UINT size);
	CbufferView CreateCbufferResource(UINT size, std::string name);
	CbufferView GetCbufferResource(std::string name);

	ComPtr<ID3D12Resource> CreateUploadStructuredBuffer(const void* data, UINT dataSize, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ);
	ComPtr<ID3D12Resource> CreateStructuredBuffer(UINT dataSize, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT);

private:

	std::unique_ptr<DirectX::DX12::GraphicsMemory> graphicsMemory;
	AaRenderSystem* renderSystem;

	std::map<std::string, CbufferData> cbuffers;
};