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
	bool done = false;
	ID3D12GraphicsCommandList* commandList{};
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
	CommandsMarker StartCommandList(CommandsData& commands);
	void ExecuteCommandList(CommandsData& commands);
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
