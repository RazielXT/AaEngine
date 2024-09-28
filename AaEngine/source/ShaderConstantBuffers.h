#pragma once

#include "GraphicsMemory.h"
#include <memory>
#include <string>
#include <map>

class AaRenderSystem;

struct CbufferView
{
	DirectX::DX12::GraphicsResource* data[2];
};

class ShaderConstantBuffers
{
public:

	ShaderConstantBuffers(AaRenderSystem* rs);
	~ShaderConstantBuffers();

	static ShaderConstantBuffers& get();

	CbufferView CreateCbufferResource(UINT size, std::string name);
	CbufferView GetCbufferResource(std::string name);

private:

	std::unique_ptr<DirectX::DX12::GraphicsMemory> graphicsMemory;
	AaRenderSystem* renderSystem;

	struct CbufferData
	{
		DirectX::DX12::GraphicsResource data[2];
	};
	std::map<std::string, CbufferData> cbuffers;
};