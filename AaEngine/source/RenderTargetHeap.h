#pragma once

#include "TextureResources.h"
#include "directx\d3dx12.h"
#include <wrl.h>

using namespace Microsoft::WRL;

class RenderTargetHeap
{
public:

	void InitRtv(ID3D12Device* device, UINT count, const wchar_t* name = nullptr);
	void InitDsv(ID3D12Device* device, UINT count, const wchar_t* name = nullptr);
	void Reset();

	void CreateRenderTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, ShaderTextureView& view);
	void CreateDepthTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle);

private:

	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	UINT rtvHandlesCount = 0;

	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	UINT dsvHandlesCount = 0;
};
