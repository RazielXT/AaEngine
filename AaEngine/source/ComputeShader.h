#pragma once

#include <string>
#include <vector>
#include "Directx.h"

struct LoadedShader;
class ShaderLibrary;
class ComputeShader;

struct ComputeShaderLibrary
{
	std::vector<ComputeShader*> shaders;

	static void Reload(ID3D12Device& device, const std::vector<const LoadedShader*>&);
};

class ComputeShader
{
	friend ComputeShaderLibrary;
public:

	ComputeShader();
	~ComputeShader();

	void init(ID3D12Device& device, const std::string& name, ShaderLibrary& shaders);
	void init(ID3D12Device& device, const std::string& name, const LoadedShader& shader);

	void reload(ID3D12Device& device);

protected:

	ComPtr<ID3D12PipelineState> pipelineState;
	ID3D12RootSignature* signature{};

	bool volatileTextures = false;

	const LoadedShader* csShader{};
	std::string name;
};