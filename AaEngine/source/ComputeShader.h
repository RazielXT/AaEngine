#pragma once

#include <string>
#include "Directx.h"

class ComputeShader
{
public:

	ComputeShader() = default;
	~ComputeShader();

	void init(ID3D12Device* device, const std::string& name);

protected:

	ComPtr<ID3D12PipelineState> pipelineState;
	ID3D12RootSignature* signature{};
};