#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include "TargetWindow.h"
#include "RenderTargetTexture.h"
#include "Upscaling.h"
#include "DescriptorManager.h"

using namespace DirectX;

struct CommandsData
{
	ID3D12GraphicsCommandList* commandList{};
	ID3D12CommandAllocator* commandAllocators[FrameCount];
	std::string name;

	void deinit();
};

struct CommandsMarker
{
	CommandsMarker(CommandsData&);
	CommandsMarker(ID3D12GraphicsCommandList* c, const char* name);
	~CommandsMarker();

	void move(const char* text);
	void mark(const char* text);
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

	CommandsData CreateCommandList(const wchar_t* name = nullptr);
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

	HANDLE fenceEvent;
	ID3D12Fence* fence;
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
