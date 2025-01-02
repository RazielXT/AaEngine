#pragma once

#include <string>
#include "Directx.h"

struct ShaderRef;
struct LoadedShader;
class ShaderLibrary;

class ComputeShader
{
public:

	ComputeShader() = default;
	~ComputeShader();

	void init(ID3D12Device& device, const std::string& name, ShaderLibrary& shaders);
	void init(ID3D12Device& device, const std::string& name, const LoadedShader& shader);

protected:

	ComPtr<ID3D12PipelineState> pipelineState;
	ID3D12RootSignature* signature{};

	bool volatileTextures = false;
};