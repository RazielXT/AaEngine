#pragma once

#include <string>
#include <wrl.h>
#include <DirectXMath.h>
#include "d3d12.h"
#include "AaTextureResources.h"

using namespace Microsoft::WRL;

class RenderDepthTargetTexture
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, int frameCount);

	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, int frameIndex);
	void PrepareAsDepthView(ID3D12GraphicsCommandList* commandList, int frameIndex);

	UINT width = 0;
	UINT height = 0;

	ComPtr<ID3D12DescriptorHeap> dsvHeap;
	ComPtr<ID3D12Resource> depthStencilTexture[2];
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandles[2];
	ShaderTextureView depthView;
	D3D12_RESOURCE_STATES dsvLastState[2]{};

protected:

	void CreateDepthBuffer(ID3D12Device* device, UINT width, UINT height, int frameCount);
};

class RenderTargetTexture : public RenderDepthTargetTexture
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, int frameCount, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
	void InitExisting(ID3D12Resource**, ID3D12Device* device, UINT width, UINT height, int frameCount, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, int frameIndex, bool clear = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, int frameIndex);
	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, int frameIndex);

	DirectX::XMFLOAT4 clearColor = { 0.55f, 0.75f, 0.9f, 1.0f };
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	ComPtr<ID3D12Resource> texture[2];
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
	ShaderTextureView textureView;
	D3D12_RESOURCE_STATES rtvLastState[2]{};

private:

	void CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, int frameCount);
	void CreateTextureBufferHandles(ID3D12Device* device, int frameCount);
};
