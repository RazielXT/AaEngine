#pragma once

#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class GpuTextureReadback
{
public:

	void init(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);

	void copyFrom(ID3D12GraphicsCommandList* commandList, ID3D12Resource* srcTexture);

	const void* map();
	void unmap();

	UINT getRowPitch() const;
	UINT getWidth() const;
	UINT getHeight() const;

	bool isInitialized() const;

private:

	ComPtr<ID3D12Resource> readbackBuffer;
	UINT width{};
	UINT height{};
	UINT rowPitch{};
	DXGI_FORMAT format{};
};
