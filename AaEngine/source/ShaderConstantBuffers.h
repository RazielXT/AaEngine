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

	ComPtr<ID3D12Resource> CreateStructuredBuffer(const void* data, UINT dataSize);

private:

	std::unique_ptr<DirectX::DX12::GraphicsMemory> graphicsMemory;
	AaRenderSystem* renderSystem;

	std::map<std::string, CbufferData> cbuffers;
};