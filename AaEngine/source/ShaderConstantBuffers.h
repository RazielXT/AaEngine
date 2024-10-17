#pragma once

#include "GraphicsMemory.h"
#include <memory>
#include <string>
#include <map>

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

private:

	std::unique_ptr<DirectX::DX12::GraphicsMemory> graphicsMemory;
	AaRenderSystem* renderSystem;

	std::map<std::string, CbufferData> cbuffers;
};