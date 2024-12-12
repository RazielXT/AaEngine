#include "RenderSystem.h"
#include "..\..\DirectX-Headers\include\directx\d3dx12.h"
#include <wrl.h>
#include "AaMaterial.h"
#include "Model.h"
#include "OgreMeshFileParser.h"
#include "SceneManager.h"
#include "AaMaterialResources.h"
#include <codecvt>
#include <pix3.h>

RenderSystem::RenderSystem(AaWindow* mWindow)
{
	this->window = mWindow;
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

	ID3D12Resource* swapChainTextures[FrameCount];
	for (UINT n = 0; n < FrameCount; n++)
	{
		swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainTextures[n]));
	}
	backbufferHeap.InitRtv(device, FrameCount, L"BackbufferRTV");

	for (int i = 0; auto& b : backbuffer)
	{
		b.InitExisting(swapChainTextures[i++], device, width, height, backbufferHeap, DXGI_FORMAT_R8G8B8A8_UNORM);
		b.SetName("Backbuffer");
	}

	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

RenderSystem::~RenderSystem()
{
	fence->Release();

	dlss.shutdown();
	swapChain->Release();
	commandQueue->Release();
	device->Release();

	CloseHandle(fenceEvent);
}

void RenderSystem::init()
{
	dlss.init(this);
}

DirectX::XMUINT2 RenderSystem::getRenderSize() const
{
	if (dlss.enabled())
		return dlss.getRenderSize();

	return getOutputSize();
}

DirectX::XMUINT2 RenderSystem::getOutputSize() const
{
	return { window->getWidth(), window->getHeight() };
}

void RenderSystem::onScreenResize()
{
	WaitForAllFrames();

	for (auto& b : backbuffer)
	{
		b = {};
	}

	auto width = window->getWidth();
	auto height = window->getHeight();

	// Resize the swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChain->GetDesc(&swapChainDesc);
	swapChain->ResizeBuffers(FrameCount, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

	ID3D12Resource* swapChainTextures[FrameCount];
	for (UINT n = 0; n < FrameCount; n++)
	{
		swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainTextures[n]));
	}
	backbufferHeap.Reset();

	for (int i = 0; auto& b : backbuffer)
	{
		b.InitExisting(swapChainTextures[i++], device, width, height, backbufferHeap, DXGI_FORMAT_R8G8B8A8_UNORM);
		b.SetName("Backbuffer");
	}

	dlss.onScreenResize();

	MoveToNextFrame();
}

void RenderSystem::WaitForCurrentFrame()
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

float RenderSystem::getMipLodBias() const
{
	float bias = 0;
	if (dlss.enabled())
		bias = std::log2f(getRenderSize().x / float(getOutputSize().x)) - 1.0f;

	return bias;
}

CommandsData RenderSystem::CreateCommandList(const wchar_t* name)
{
	CommandsData data;

	for (UINT n = 0; n < FrameCount; n++)
	{
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&data.commandAllocators[n]));
	}

	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, data.commandAllocators[frameIndex], nullptr, IID_PPV_ARGS(&data.commandList));
	data.commandList->Close();

	if (name)
	{
		data.commandList->SetName(name);
		data.name = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(name);
	}

	return data;
}

CommandsMarker RenderSystem::StartCommandList(CommandsData& commands)
{
	// Reset command allocator and command list
	commands.commandAllocators[frameIndex]->Reset();
	commands.commandList->Reset(commands.commandAllocators[frameIndex], nullptr);

	auto& descriptors = DescriptorManager::get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptors.mainDescriptorHeap, descriptors.samplerHeap };
	commands.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	return { commands };
}

void RenderSystem::ExecuteCommandList(CommandsData& commands)
{
	// Close the command list and execute it
	commands.commandList->Close();

	ID3D12CommandList* commandLists[] = { commands.commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
}

void RenderSystem::Present()
{
	swapChain->Present(1, 0);
}

void RenderSystem::EndFrame()
{
	MoveToNextFrame();
}

void RenderSystem::WaitForAllFrames()
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
void RenderSystem::MoveToNextFrame()
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
		commandList = nullptr;
	}
}

CommandsMarker::CommandsMarker(CommandsData& c)
{
	if (!c.name.empty())
	{
		commandList = c.commandList;
		PIXBeginEvent(commandList, 0, c.name.c_str());
	}
}

CommandsMarker::CommandsMarker(ID3D12GraphicsCommandList* c, const char* text) : commandList(c)
{
	PIXBeginEvent(commandList, 0, text);
}

CommandsMarker::~CommandsMarker()
{
	close();
}

void CommandsMarker::move(const char* text)
{
	close();
	done = false;
	PIXBeginEvent(commandList, 0, text);
}

void CommandsMarker::mark(const char* text)
{
	if (!done && commandList)
		PIXSetMarker(commandList, 0, text);
}

void CommandsMarker::close()
{
	if (!done && commandList)
	{
		done = true;
		PIXEndEvent(commandList);
	}
}
