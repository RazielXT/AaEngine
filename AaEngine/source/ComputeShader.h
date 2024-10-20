#pragma once

#include <string>
#include "directx/d3d12.h"

class ComputeShader
{
public:

	ComputeShader() = default;

	void init(ID3D12Device* device, const std::string& name);

private:

};