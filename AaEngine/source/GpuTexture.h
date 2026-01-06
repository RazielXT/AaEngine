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
class RenderTargetTextures;

class GpuTexture2D : public GpuTextureResource
{
	friend RenderTargetTexturesView;
	friend RenderTargetTextures;
public:

	struct InitParams
	{
		D3D12_RESOURCE_FLAGS flags;
		UINT mipLevels = 1;
	};
	void InitUAV(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, InitParams&& params = {});

	void InitRenderTarget(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, InitParams&& params = {});
	void InitExistingRenderTarget(ID3D12Resource*, ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, DXGI_FORMAT format);

	void InitDepth(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE, float clearValue = 0.f);

	void PrepareAsRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = false);
 	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
 	void PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);
	void PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true);
	void ClearDepth(ID3D12GraphicsCommandList* commandList);

private:

	void CreateTextureBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES initialState, const InitParams&);
	void CreateDepthBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES initialState);

	float depthClearValue = 0.f;
};

class GpuTexture3D : public GpuTextureResource
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, UINT depth, DXGI_FORMAT format);

	std::vector<ShaderTextureViewUAV> uav;
};

struct GpuTextureStates
{
	GpuTexture2D* texture{};
	D3D12_RESOURCE_STATES previousState{};
	D3D12_RESOURCE_STATES state{};
	D3D12_RESOURCE_STATES nextState{};

	static void Transition(ID3D12GraphicsCommandList* commandList, const std::vector<GpuTextureStates>&);
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

	void Init(ID3D12Device* device, std::vector<GpuTexture2D*> t, GpuTexture2D* d);

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, const std::vector<GpuTextureStates>&, bool clear = true, UINT flags = TransitionFlags::DepthPrepareClearWrite);

	std::vector<DXGI_FORMAT> formats;
	UINT width;
	UINT height;

protected:

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
	bool contiguous{};
	bool depth{};
};

class RenderTargetTextures : public RenderTargetTexturesView
{
public:

	void Init(ID3D12Device* device, UINT width, UINT height, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& format, D3D12_RESOURCE_STATES state, bool depthBuffer = true, D3D12_RESOURCE_STATES initialDepthState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
	void SetName(const std::string& name);

	void PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear = true, UINT flags = TransitionFlags::DepthPrepareClearWrite);
	void TransitionFromTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, bool depth = true);
	void PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from);

	std::vector<GpuTexture2D> textures;
	GpuTexture2D depth;
};

struct TextureStatePair
{
	TextureStatePair() = default;
	TextureStatePair(const GpuTextureStates& s) : texture(s.texture), currentState(s.previousState) {}
	TextureStatePair(GpuTextureResource* s, D3D12_RESOURCE_STATES state) : texture(s), currentState(state) {}

	GpuTextureResource* texture{};
	D3D12_RESOURCE_STATES currentState{};

	void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to);
};

template<UINT MAX>
struct TextureTransitions
{
	UINT c = 0;
	CD3DX12_RESOURCE_BARRIER barriers[MAX];

	TextureTransitions() = default;
	TextureTransitions(const std::vector<GpuTextureStates>& states, ID3D12GraphicsCommandList* commandList)
	{
		for (auto& state : states)
			add(state);
		push(commandList);
	}
	TextureTransitions(const std::vector<GpuTextureStates*>& states, ID3D12GraphicsCommandList* commandList)
	{
		for (auto& state : states)
			add(*state);
		push(commandList);
	}

	void add(const GpuTextureResource* texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		if (from == to)
			return;

		barriers[c++] = CD3DX12_RESOURCE_BARRIER::Transition(
			texture->texture.Get(),
			from,
			to);
	}
	void add(const GpuTextureResource& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		add(&texture, from, to);
	}
	void add(ID3D12Resource* texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		if (from == to)
			return;

		barriers[c++] = CD3DX12_RESOURCE_BARRIER::Transition(
			texture,
			from,
			to);
	}
	void add(const GpuTextureStates& state)
	{
		add(state.texture, state.previousState, state.state);
	}
	void add(TextureStatePair& state, D3D12_RESOURCE_STATES to)
	{
		add(state.texture, state.currentState, to);

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
