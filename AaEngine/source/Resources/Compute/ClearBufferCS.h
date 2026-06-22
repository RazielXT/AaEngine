#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Utils/Directx.h"
#include "Resources/ResourcesView.h"

class ClearBufferComputeShader : public ComputeShader
{
public:

	ClearBufferComputeShader() = default;

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* buffer, UINT size);
};

class ClearTextureComputeShader : public ComputeShader
{
public:

	ClearTextureComputeShader() = default;

	void dispatch(ID3D12GraphicsCommandList* commandList, const ShaderTextureView& texture, DirectX::XMUINT3 size);
};
