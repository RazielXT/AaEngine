#include "AaRenderSystem.h"
#include "..\..\DirectX-Headers\include\directx\d3dx12.h"
#include <wrl.h>
#include "AaMaterial.h"
#include "Model.h"
#include "OgreMeshFileParser.h"
#include "SceneManager.h"
#include "AaMaterialResources.h"

AaRenderSystem::AaRenderSystem(AaWindow* mWindow)
{
	this->mWindow = mWindow;
	mWindow->listeners.push_back(this);

	HWND hwnd = mWindow->getHwnd();

	auto width = mWindow->getWidth();
	auto height = mWindow->getHeight();

	ComPtr<IDXGIFactory4> factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	{
		IDXGIAdapter1* hardwareAdapter;
		factory->EnumAdapters1(0, &hardwareAdapter);
		D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	factory->CreateSwapChainForHwnd(
		commandQueue,        // Swap chain needs the queue so that it can force a flush on it.
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&swapChain
	);

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	ID3D12Resource* swapChainTextures[2];
	for (UINT n = 0; n < FrameCount; n++)
	{
		swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainTextures[n]));
	}
	backbufferHeap.Init(device, 1, FrameCount, L"BackbufferRTV");
	backbuffer.InitExisting(swapChainTextures, device, width, height, FrameCount, backbufferHeap);
	backbuffer.SetName(L"Backbuffer");

	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

AaRenderSystem::~AaRenderSystem()
{
	fence->Release();

	swapChain->Release();
	commandQueue->Release();
	device->Release();

	CloseHandle(fenceEvent);
}

void AaRenderSystem::onScreenResize()
{
	WaitForAllFrames();

	backbuffer = {};

	auto width = mWindow->getWidth();
	auto height = mWindow->getHeight();

	// Resize the swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChain->GetDesc(&swapChainDesc);
	swapChain->ResizeBuffers(FrameCount, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

	ID3D12Resource* swapChainTextures[2];
	for (UINT n = 0; n < FrameCount; n++)
	{
		swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainTextures[n]));
	}
	backbufferHeap.Reset();
	backbuffer.InitExisting(swapChainTextures, device, width, height, FrameCount, backbufferHeap);
	backbuffer.SetName(L"Backbuffer");

	MoveToNextFrame();
}

void AaRenderSystem::WaitForCurrentFrame()
{
	const UINT64 currentFenceValue = fenceValues[frameIndex];
	commandQueue->Signal(fence, currentFenceValue);

	//if (fence->GetCompletedValue() < currentFenceValue)
	{
		fence->SetEventOnCompletion(currentFenceValue, fenceEvent);
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}

	fenceValues[frameIndex]++;
}

CommandsData AaRenderSystem::CreateCommandList(const wchar_t* name)
{
	CommandsData data;

	for (UINT n = 0; n < FrameCount; n++)
	{
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&data.commandAllocators[n]));
	}

	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, data.commandAllocators[frameIndex], nullptr, IID_PPV_ARGS(&data.commandList));
	data.commandList->Close();

	if (name)
		data.commandList->SetName(name);

	return data;
}

void AaRenderSystem::StartCommandList(CommandsData commands)
{
	// Reset command allocator and command list
	commands.commandAllocators[frameIndex]->Reset();
	commands.commandList->Reset(commands.commandAllocators[frameIndex], nullptr);

	ID3D12DescriptorHeap* descriptorHeaps[] = { DescriptorManager::get().mainDescriptorHeap[frameIndex]};
	commands.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void AaRenderSystem::ExecuteCommandList(CommandsData commands)
{
	// Close the command list and execute it
	commands.commandList->Close();

	ID3D12CommandList* commandLists[] = { commands.commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
}

void AaRenderSystem::Present()
{
	swapChain->Present(1, 0);
}

void AaRenderSystem::EndFrame()
{
	MoveToNextFrame();
}

void AaRenderSystem::WaitForAllFrames()
{
	for (int i = 0; i < FrameCount; i++)
	{
		const UINT64 currentFenceValue = fenceValues[i];
		commandQueue->Signal(fence, currentFenceValue);

		//if (fence->GetCompletedValue() < currentFenceValue)
		{
			fence->SetEventOnCompletion(currentFenceValue, fenceEvent);
			WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
		}

		fenceValues[i]++;
	}
}

// Prepare to render the next frame.
void AaRenderSystem::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = fenceValues[frameIndex];
	commandQueue->Signal(fence, currentFenceValue);

	// Update the frame index.
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (fence->GetCompletedValue() < fenceValues[frameIndex])
	{
		fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent);
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	fenceValues[frameIndex] = currentFenceValue + 1;
}

void CommandsData::deinit()
{
	if (commandList)
	{
		commandList->Release();

		for (auto& a : commandAllocators)
		{
			a->Release();
		}
	}
}
