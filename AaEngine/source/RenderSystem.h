#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include "TargetWindow.h"
#include "RenderTargetTexture.h"
#include "Upscaling.h"
#include "DescriptorManager.h"
#include "PixColor.h"

using namespace DirectX;

struct CommandsData
{
	ID3D12GraphicsCommandList* commandList{};
	ID3D12CommandAllocator* commandAllocators[FrameCount];
	std::string name;
	PixColor color;

	void deinit();
};

struct GlobalQueueMarker
{
	GlobalQueueMarker(ID3D12CommandQueue*, const char* name);
	~GlobalQueueMarker();

private:

	ID3D12CommandQueue* queue{};
};

struct CommandsMarker
{
	CommandsMarker(CommandsData&);
	CommandsMarker(ID3D12GraphicsCommandList* c, const char* name, PixColor color);
	~CommandsMarker();

	void move(const char* text, PixColor);
	void mark(const char* text, PixColor);
	void close();

private:
	bool closed = false;
	ID3D12GraphicsCommandList* commandList{};
};

class RenderCore
{
public:

	RenderCore();
	~RenderCore();

	UINT frameIndex;

	ID3D12Device* device;
	ID3D12CommandQueue* commandQueue;
	ID3D12CommandQueue* copyQueue;
	ID3D12CommandQueue* computeQueue;

	CommandsData CreateCommandList(const wchar_t* name, PixColor, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
	CommandsMarker StartCommandList(CommandsData& commands);
	void StartCommandListNoMarker(CommandsData& commands);
	CommandsMarker StartCommandList(CommandsData& commands, ID3D12DescriptorHeap* heap);

	void ExecuteCommandList(CommandsData& commands);

	HRESULT Present(bool vsync = true);
	void EndFrame();

	void WaitForAllFrames();
	void WaitForCurrentFrame();

	RenderTargetHeap rtvHeap;
	RenderTargetTexture backbuffer[FrameCount];

	void initializeSwapChain(const TargetWindow& window);

	void resize(UINT width, UINT height);

private:

	IDXGISwapChain3* swapChain;

	void MoveToNextFrame();

	HANDLE commandFenceEvent;
	ID3D12Fence* commandFence;

	HANDLE computeFenceEvent;
	ID3D12Fence* computeFence;

	UINT64 fenceValues[FrameCount];
};

class RenderSystem : public ViewportListener
{
public:

	RenderSystem(TargetViewport& viewport);
	~RenderSystem();

	XMUINT2 getRenderSize() const;
	XMUINT2 getOutputSize() const;

	RenderCore core;

	Upscaling upscale;

	TargetViewport& viewport;

private:

	void onViewportResize(UINT, UINT) override;
	void onScreenResize(UINT, UINT) override;
};
