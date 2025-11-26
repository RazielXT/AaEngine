#pragma once

#include "Directx.h"
#include "ResourceUploadBatch.h"

namespace TextureUtils
{
	UINT BitsPerPixel(DXGI_FORMAT fmt);

	ComPtr<ID3D12Resource> InitializeTextureBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		ID3D12Resource* texture,
		const void* initData,
		UINT width,
		UINT height,
		DXGI_FORMAT format,
		D3D12_RESOURCE_STATES state);

	void CopyTextureBuffer(
		ID3D12GraphicsCommandList* commandList,
		ID3D12Resource* textureSrc,
		D3D12_RESOURCE_STATES stateSrc,
		ID3D12Resource* textureDst,
		D3D12_RESOURCE_STATES stateDst);

	ComPtr<ID3D12Resource> CreateUploadTexture(
		ID3D12Device* device,
		DirectX::ResourceUploadBatch& batch,
		void* data,
		UINT width,
		UINT height,
		DXGI_FORMAT format,
		D3D12_RESOURCE_STATES state);
}
