#pragma once

#include <string>
#include "Directx.h"

class ComputeShader
{
public:

	ComputeShader() = default;

	void init(ID3D12Device* device, const std::string& name);

protected:

	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12RootSignature> signature;
};