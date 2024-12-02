#pragma once

#include "AaTextureResources.h"
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

	void CreateRenderTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle);
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

	enum Flags { AllowRenderTarget = 1, AllowUAV = 2 };
	void Init(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, Flags flags = AllowRenderTarget);
	void InitDepth(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	void InitExisting(ID3D12Resource*, ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format);

	void SetName(const wchar_t* name);

 	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = false);
 	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
 	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
// 	void TransitionTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, D3D12_RESOURCE_STATES from);

	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true);
	void ClearDepth(ID3D12GraphicsCommandList* commandList);

private:

	void CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT, D3D12_RESOURCE_STATES, Flags flags);
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
		SkipTransitionDepth = 8,
		DepthPrepareClearWrite = TransitionFlags::UseDepth | TransitionFlags::ClearDepth,
		DepthPrepareRead = TransitionFlags::UseDepth | TransitionFlags::ReadOnlyDepth,
	};
}

class RenderTargetTexturesView
{
public:

	void Init();

	std::vector<RenderTargetTextureState> texturesState;
	RenderTargetTextureState depthState;

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, bool clear = true, UINT flags = TransitionFlags::DepthPrepareClearWrite);
	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true, UINT flags = TransitionFlags::DepthPrepareClearWrite);

	void TransitionFromTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, bool depth = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);

	std::vector<DXGI_FORMAT> formats;

private:

	UINT width;
	UINT height;

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
};

class RenderTargetTextures : public RenderTargetTexturesView
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& format, D3D12_RESOURCE_STATES state, bool depthBuffer = true, D3D12_RESOURCE_STATES initialDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	void SetName(const wchar_t* name);

	std::vector<RenderTargetTexture> textures;
	RenderTargetTexture depth;
};

template<UINT MAX>
struct RenderTargetTransitions
{
	UINT c = 0;
	CD3DX12_RESOURCE_BARRIER barriers[MAX];

	void add(RenderTargetTextureState& state, D3D12_RESOURCE_STATES to)
	{
		barriers[c] = CD3DX12_RESOURCE_BARRIER::Transition(
			state.texture->texture.Get(),
			state.previousState,
			to);

		state.previousState = to;
		c++;
	}
	void addAndPush(RenderTargetTextureState& state, D3D12_RESOURCE_STATES to, ID3D12GraphicsCommandList* commandList)
	{
		add(state, to);
		push(commandList);
	}
	void push(ID3D12GraphicsCommandList* commandList)
	{
		commandList->ResourceBarrier(c, barriers);
		c = 0;
	}
};
