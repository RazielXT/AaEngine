#pragma once

#include "TextureResources.h"
#include "d3d12.h"
#include "directx\d3dx12.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
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

class RenderTextureInfo
{
public:

	RenderTextureInfo() = default;

	ComPtr<ID3D12Resource> texture;
	ShaderTextureView view;

	DXGI_FORMAT format{};

	UINT width = 0;
	UINT height = 0;
	UINT arraySize = 1;
};

class RenderTargetTexture : public RenderTextureInfo
{
public:
	~RenderTargetTexture();

	enum Flags { AllowRenderTarget = 1, AllowUAV = 2 };
	void Init(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, UINT flags = AllowRenderTarget);
	void InitUAV(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state);
	void InitDepth(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	void InitExisting(ID3D12Resource*, ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format);
	void Resize(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, D3D12_RESOURCE_STATES state);

	void SetName(const std::string& name);
	std::string name;

 	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = false);
 	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
 	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
 	void TransitionTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true);
	void ClearDepth(ID3D12GraphicsCommandList* commandList);

	float DepthClearValue = 0.f;

private:

	void ResizeTextureBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES state);
	void CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT, D3D12_RESOURCE_STATES, UINT flags);
	void CreateDepthBuffer(ID3D12Device* device, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState);
};

struct RenderTargetTextureState
{
	RenderTargetTexture* texture{};
	D3D12_RESOURCE_STATES previousState{};
};

namespace TransitionFlags
{
	enum Flags
	{
		NoDepth = 0,
		UseDepth = 1,
		ClearDepth = 2,
		ReadOnlyDepth = 4,
		DepthSkipTransition = 8,
		DepthPrepareClearWrite = TransitionFlags::UseDepth | TransitionFlags::ClearDepth,
		DepthPrepareRead = TransitionFlags::UseDepth | TransitionFlags::ReadOnlyDepth,
		DepthContinue = TransitionFlags::UseDepth | TransitionFlags::DepthSkipTransition,
	};
}

class RenderTargetTexturesView
{
public:

	void Init(ID3D12Device* device);

	std::vector<RenderTargetTextureState> texturesState;
	RenderTargetTextureState depthState;

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, bool clear = true, UINT flags = TransitionFlags::DepthPrepareClearWrite);
	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true, UINT flags = TransitionFlags::DepthPrepareClearWrite);

	void TransitionFromTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, bool depth = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);

	std::vector<DXGI_FORMAT> formats;

private:

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;

	UINT width;
	UINT height;
	bool contiguous{};
};

class RenderTargetTextures : public RenderTargetTexturesView
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& format, D3D12_RESOURCE_STATES state, bool depthBuffer = true, D3D12_RESOURCE_STATES initialDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	void SetName(const std::string& name);

	std::vector<RenderTargetTexture> textures;
	RenderTargetTexture depth;
};

template<UINT MAX>
struct RenderTargetTransitions
{
	UINT c = 0;
	CD3DX12_RESOURCE_BARRIER barriers[MAX];

	void addConst(const RenderTargetTextureState& state, D3D12_RESOURCE_STATES to)
	{
		if (state.previousState == to)
			return;

		barriers[c] = CD3DX12_RESOURCE_BARRIER::Transition(
			state.texture->texture.Get(),
			state.previousState,
			to);

		c++;
	}
	void add(RenderTargetTextureState& state, D3D12_RESOURCE_STATES to)
	{
		addConst(state, to);

		state.previousState = to;
	}
	void addAndPush(RenderTargetTextureState& state, D3D12_RESOURCE_STATES to, ID3D12GraphicsCommandList* commandList)
	{
		add(state, to);
		push(commandList);
	}
	void push(ID3D12GraphicsCommandList* commandList)
	{
		if (c)
			commandList->ResourceBarrier(c, barriers);
		c = 0;
	}
};
