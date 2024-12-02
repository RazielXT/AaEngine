#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include "AaWindow.h"
#include <map>
#include "RenderTargetTexture.h"

using namespace DirectX;

class SceneManager;
class AaCamera;

struct CommandsData
{
	ID3D12GraphicsCommandList* commandList{};
	ID3D12CommandAllocator* commandAllocators[2];

	void deinit();
};

class RenderSystem : public ScreenListener
{
public:

	RenderSystem(AaWindow* mWindow);
	~RenderSystem();

	static const UINT FrameCount = 2;

 	AaWindow* getWindow() { return mWindow; }

	ID3D12Device* device;
	ID3D12CommandQueue* commandQueue;
	IDXGISwapChain3* swapChain;

	UINT rtvDescriptorSize = 0;
	UINT dsvDescriptorSize = 0;

	UINT frameIndex;
	HANDLE fenceEvent;
	ID3D12Fence* fence;
	UINT64 fenceValues[2];

	CommandsData CreateCommandList(const wchar_t* name = nullptr);
	void StartCommandList(CommandsData commands);
	void ExecuteCommandList(CommandsData commands);
	void Present();
	void EndFrame();

	void WaitForAllFrames();
	void WaitForCurrentFrame();

	RenderTargetTexture backbuffer[FrameCount];

private:

	RenderTargetHeap backbufferHeap;

	void MoveToNextFrame();

	void onScreenResize() override;

	AaWindow* mWindow;
};
