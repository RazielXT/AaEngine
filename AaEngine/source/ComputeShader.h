#pragma once

#include <string>
#include "Directx.h"

struct ShaderRef;
struct LoadedShader;

class ComputeShader
{
public:

	ComputeShader() = default;
	~ComputeShader();

	void init(ID3D12Device* device, const std::string& name);
	void init(ID3D12Device* device, const std::string& name, const ShaderRef& ref);
	void init(ID3D12Device* device, const std::string& name, LoadedShader* shader);

protected:

	ComPtr<ID3D12PipelineState> pipelineState;
	ID3D12RootSignature* signature{};

	bool volatileTextures = false;
};