#pragma once

#include "TextureResources.h"
#include "RenderTargetHeap.h"
#include <vector>
#include <string>

class GpuTextureResource
{
public:

	GpuTextureResource() = default;
	~GpuTextureResource();

	ComPtr<ID3D12Resource> texture;
	ShaderTextureView view;

	DXGI_FORMAT format{};

	UINT width = 0;
	UINT height = 0;
	UINT depthOrArraySize = 1;

	void SetName(const std::string& name);
	std::string name;

	void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
};

class RenderTargetTexturesView;

class GpuTexture2D : public GpuTextureResource
{
	friend RenderTargetTexturesView;
public:

	enum Flags { AllowRenderTarget = 1, AllowUAV = 2 };
	void Init(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, UINT flags = AllowRenderTarget);
	void InitDepth(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE, float clearValue = 0.f);
	void InitExisting(ID3D12Resource*, ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format);
	void InitUAV(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state);

	void PrepareAsRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = false);
 	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
 	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true);
	void ClearDepth(ID3D12GraphicsCommandList* commandList);

private:

	void CreateTextureBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES initialState, UINT flags);
	void CreateDepthBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES initialState);

	float depthClearValue = 0.f;
};

class GpuTexture3D : public GpuTextureResource
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, UINT depth, DXGI_FORMAT format);

	std::vector<ShaderUAV> uav;
};

struct RenderTargetTextureState
{
	GpuTexture2D* texture{};
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

	std::vector<GpuTexture2D> textures;
	GpuTexture2D depth;
};

struct TextureStatePair
{
	TextureStatePair() = default;
	TextureStatePair(const RenderTargetTextureState& s) : texture(s.texture), currentState(s.previousState) {}
	TextureStatePair(GpuTextureResource* s, D3D12_RESOURCE_STATES state) : texture(s), currentState(state) {}

	GpuTextureResource* texture{};
	D3D12_RESOURCE_STATES currentState{};

	void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to);
};

template<UINT MAX>
struct RenderTargetTransitions
{
	UINT c = 0;
	CD3DX12_RESOURCE_BARRIER barriers[MAX];

	void addConst(const TextureStatePair& state, D3D12_RESOURCE_STATES to)
	{
		if (state.currentState == to)
			return;

		barriers[c] = CD3DX12_RESOURCE_BARRIER::Transition(
			state.texture->texture.Get(),
			state.currentState,
			to);

		c++;
	}
	void add(TextureStatePair& state, D3D12_RESOURCE_STATES to)
	{
		addConst(state, to);

		state.currentState = to;
	}
	void addAndPush(TextureStatePair& state, D3D12_RESOURCE_STATES to, ID3D12GraphicsCommandList* commandList)
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
