#pragma once

#include "AaTextureResources.h"
#include "d3d12.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <wrl.h>

using namespace Microsoft::WRL;

class RenderDepthTargetTexture
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, UINT frameCount, UINT arraySize = 1);

	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
	void PrepareAsDepthView(ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void SetName(const wchar_t* name);

	UINT width = 0;
	UINT height = 0;
	UINT arraySize = 1;

	ComPtr<ID3D12Resource> depthStencilTexture[2];
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandles[2]{};
	ShaderTextureView depthView;
	D3D12_RESOURCE_STATES dsvLastState[2]{};

protected:

	void CreateDepthBuffer(ID3D12Device* device, UINT frameCount, UINT arraySize);

	ComPtr<ID3D12DescriptorHeap> dsvHeap;
};

class RenderTargetHeap
{
public:

	void Init(ID3D12Device* device, UINT count, UINT frameCount, const wchar_t* name = nullptr);
	void Reset();

	void CreateRenderTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle);

private:

	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	UINT handlesCount = 0;
};

class RenderTargetTexture : public RenderDepthTargetTexture
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, UINT frameCount, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& formats, bool depthBuffer = true);
	void InitExisting(ID3D12Resource**, ID3D12Device* device, UINT width, UINT height, UINT frameCount, RenderTargetHeap& heap, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, bool clear = true, bool depth = true, bool clearDepth = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, UINT frameIndex);
	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void SetName(const wchar_t* name);

	DirectX::XMFLOAT4 clearColor = { 0.55f, 0.75f, 0.9f, 1.0f };

	struct Texture
	{
		ComPtr<ID3D12Resource> texture[2];
		ShaderTextureView textureView;
	};
	std::vector<Texture> textures;

	std::vector<DXGI_FORMAT> formats;

private:

	void CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, UINT frameCount, Texture& t, DXGI_FORMAT);

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles[2]{};
	D3D12_RESOURCE_STATES rtvLastState[2]{ D3D12_RESOURCE_STATE_COMMON };
};
