#pragma once

#include "Directx.h"

struct ShaderTextureView
{
	ShaderTextureView() = default;

	D3D12_CPU_DESCRIPTOR_HANDLE handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandle{};
	UINT heapIndex{};
	UINT srvHeapIndex{};
	UINT uavHeapIndex{};
};

class ShaderViewUAV
{
public:

	D3D12_GPU_DESCRIPTOR_HANDLE handle{};
	UINT heapIndex{};
};

class ShaderTextureViewUAV : public ShaderViewUAV
{
public:

	ShaderTextureViewUAV() = default;

	UINT mipLevel{};
};
