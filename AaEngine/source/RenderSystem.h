#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include "TargetWindow.h"
#include "GpuTexture.h"
#include "Upscaling.h"
#include "DescriptorManager.h"
#include "PixColor.h"

using namespace DirectX;

struct CommandsData
{
	ID3D12GraphicsCommandList* commandList{};
	ID3D12CommandAllocator* commandAllocators[FrameCount]{};
	std::string name;
	PixColor color{};

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

struct ColorSpace
{
	DXGI_FORMAT outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	enum Type { SRGB, HDR10 } type = SRGB;
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
	GpuTexture2D backbuffer[FrameCount];

	bool IsDisplayHDR() const;
	DXGI_OUTPUT_DESC1 GetDisplayDesc() const;

	void initializeSwapChain(const TargetWindow& window, ColorSpace);

	void resize(UINT width, UINT height);

private:

	ComPtr<IDXGIAdapter1> hardwareAdapter;

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
