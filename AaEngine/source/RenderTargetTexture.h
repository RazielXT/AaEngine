#pragma once

#include "AaTextureResources.h"
#include "d3d12.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <wrl.h>

using namespace Microsoft::WRL;

class RenderTargetInfo
{
public:

	RenderTargetInfo() = default;

	struct Texture
	{
		ComPtr<ID3D12Resource> texture[2];
		ShaderTextureView textureView;
	};
	std::vector<Texture> textures;

	std::vector<DXGI_FORMAT> formats;

	UINT width = 0;
	UINT height = 0;
	UINT arraySize = 1;
};

class RenderDepthTargetTexture : public RenderTargetInfo
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, UINT frameCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE, UINT arraySize = 1);
	void ClearDepth(ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from);
	void PrepareAsDepthView(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from);
	void TransitionDepth(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES to, D3D12_RESOURCE_STATES from);

	void SetName(const wchar_t* name);

	ComPtr<ID3D12Resource> depthStencilTexture[2];
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandles[2]{};
	ShaderTextureView depthView;

protected:

	void CreateDepthBuffer(ID3D12Device* device, UINT frameCount, D3D12_RESOURCE_STATES initialState, UINT arraySize);

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

	void Init(ID3D12Device* device, UINT width, UINT height, UINT frameCount, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& formats, D3D12_RESOURCE_STATES state, bool depthBuffer = true, D3D12_RESOURCE_STATES initialDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	void InitExisting(ID3D12Resource**, ID3D12Device* device, UINT width, UINT height, UINT frameCount, RenderTargetHeap& heap, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	void PrepareAsSingleTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, UINT textureIndex, D3D12_RESOURCE_STATES from, bool clear = true, bool depth = true, bool clearDepth = true);
	void PrepareAsSingleView(ID3D12GraphicsCommandList* commandList, UINT frameIndex, UINT textureIndex, D3D12_RESOURCE_STATES from);

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from, bool clear = true, bool depth = true, bool clearDepth = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from);
	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from);
	void TransitionTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES to, D3D12_RESOURCE_STATES from);

	void SetName(const wchar_t* name);

	DirectX::XMFLOAT4 clearColor = { 0.55f, 0.75f, 0.9f, 1.0f };

private:

	void CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, UINT frameCount, Texture& t, DXGI_FORMAT, D3D12_RESOURCE_STATES);

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles[2]{};
};

struct RenderTargetTextureView
{
	RenderTargetTextureView() = default;
	RenderTargetTextureView(RenderTargetTexture&);
	RenderTargetTextureView(RenderTargetTexture&, UINT index);

	void SetDepth(RenderDepthTargetTexture*);

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from, bool clear = true, bool depth = true, bool clearDepth = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from);
	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, UINT frameIndex, D3D12_RESOURCE_STATES from);

private:

	const UINT IndexAll = 100;
	UINT index = IndexAll;

	RenderTargetTexture* target{};
	RenderDepthTargetTexture* targetDepth{};
};