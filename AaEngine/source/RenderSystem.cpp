#include "RenderSystem.h"
#include "directx\d3dx12.h"
#include <wrl.h>
#include "Material.h"
#include "Model.h"
#include "OgreMeshFileParser.h"
#include "SceneManager.h"
#include "MaterialResources.h"
#include <pix3.h>
#include "FileLogger.h"
#include "StringUtils.h"

ComPtr<IDXGIAdapter1> GetHardwareAdapter(IDXGIFactory4* pFactory)
{
	ComPtr<IDXGIAdapter1> bestAdapter;
	ComPtr<IDXGIAdapter1> fallbackAdapter;

	for (UINT adapterIndex = 0; ; ++adapterIndex)
	{
		ComPtr<IDXGIAdapter1> pAdapter;
		if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter))
		{
			break;
		}

		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
		{
			// Prefer dedicated GPUs
			if (desc.DedicatedVideoMemory > 0)
			{
				bestAdapter = pAdapter;
				break;
			}
			if (!fallbackAdapter)
			{
				fallbackAdapter = pAdapter;
			}
		}
	}

	if (bestAdapter)
	{
		return bestAdapter;
	}
	else
	{
		return fallbackAdapter;
	}
}


RenderCore::RenderCore()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		FileLogger::logError("Failed to initialize COM");
		return;
	}

	ComPtr<IDXGIFactory4> factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	{
		auto hardwareAdapter = GetHardwareAdapter(factory.Get());
		auto hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

		if (FAILED(hr))
		{
			auto err = "D3D12CreateDevice failed";
			FileLogger::logErrorD3D(err, hr);
			throw std::invalid_argument(err);
		}

		D3D12_FEATURE_DATA_SHADER_MODEL SM;
		SM.HighestShaderModel = D3D_SHADER_MODEL_6_7;
		device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &SM, sizeof(SM));
		if (SM.HighestShaderModel < D3D_SHADER_MODEL_6_6)
		{
			auto err = std::format("D3D12Device shader support is {:#x}", (int)SM.HighestShaderModel);
			FileLogger::logError(err);
			throw std::invalid_argument(err);
		}

#ifndef NDEBUG
		ComPtr<ID3D12InfoQueue> pInfoQueue;
		device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
		if (pInfoQueue)
		{
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			//pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		}
#endif
	}

	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
	}
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

		device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyQueue));
	}

	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

RenderCore::~RenderCore()
{
	fence->Release();

	swapChain->Release();
	copyQueue->Release();
	commandQueue->Release();
	device->Release();

	CloseHandle(fenceEvent);
}

RenderSystem::RenderSystem(TargetViewport& v) : viewport(v), upscale(*this)
{
	viewport.listeners.push_back(this);
}

RenderSystem::~RenderSystem()
{
	upscale.shutdown();
}

DirectX::XMUINT2 RenderSystem::getRenderSize() const
{
	if (upscale.enabled())
		return upscale.getRenderSize();

	return getOutputSize();
}

DirectX::XMUINT2 RenderSystem::getOutputSize() const
{
	return { viewport.getWidth(), viewport.getHeight() };
}

void RenderSystem::onViewportResize(UINT width, UINT height)
{
	core.WaitForAllFrames();

	upscale.onResize();

// 	if (!outputIsBackbuffer)
// 	{
// 		for (int i = 0; auto & b : output.texture)
// 		{
// 			b.Resize(core.device, width, height, output.heap, D3D12_RESOURCE_STATE_COMMON);
// 		}
// 	}
}

void RenderSystem::onScreenResize(UINT width, UINT height)
{
	core.WaitForAllFrames();

	core.resize(width, height);
}

void RenderCore::initializeSwapChain(const TargetWindow& window)
{
	auto width = window.getWidth();
	auto height = window.getHeight();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGIFactory4> factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	factory->CreateSwapChainForHwnd(
		commandQueue,
		window.getHwnd(),
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
	rtvHeap.InitRtv(device, FrameCount, L"BackbufferRTV");

	for (int i = 0; auto & b : backbuffer)
	{
		b.InitExisting(swapChainTextures[i++], device, width, height, rtvHeap, DXGI_FORMAT_R8G8B8A8_UNORM);
		b.SetName("Backbuffer");
	}
}

void RenderCore::resize(UINT width, UINT height)
{
	for (auto & b : backbuffer)
	{
		b = {};
	}

	// Resize the swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChain->GetDesc(&swapChainDesc);
	swapChain->ResizeBuffers(FrameCount, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

	ID3D12Resource* swapChainTextures[FrameCount];
	for (UINT n = 0; n < FrameCount; n++)
	{
		swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainTextures[n]));
	}
	rtvHeap.Reset();

	for (int i = 0; auto& b : backbuffer)
	{
		b.InitExisting(swapChainTextures[i++], device, width, height, rtvHeap, DXGI_FORMAT_R8G8B8A8_UNORM);
		b.SetName("Backbuffer");
	}

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	for (auto& f : fenceValues)
		f = 0;

	MoveToNextFrame();
}

void RenderCore::WaitForCurrentFrame()
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

CommandsData RenderCore::CreateCommandList(const wchar_t* name, PixColor c)
{
	CommandsData data;

	for (auto& commandAllocator : data.commandAllocators)
	{
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	}

	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, data.commandAllocators[frameIndex], nullptr, IID_PPV_ARGS(&data.commandList));
	data.commandList->Close();

	if (name)
	{
		data.commandList->SetName(name);
		data.name = as_string(name);
		data.color = c;
	}

	return data;
}

CommandsMarker RenderCore::StartCommandList(CommandsData& commands)
{
	StartCommandListNoMarker(commands);

	return { commands };
}

void RenderCore::StartCommandListNoMarker(CommandsData& commands)
{
	// Reset command allocator and command list
	commands.commandAllocators[frameIndex]->Reset();
	commands.commandList->Reset(commands.commandAllocators[frameIndex], nullptr);

	auto& descriptors = DescriptorManager::get();
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptors.mainDescriptorHeap, descriptors.samplerHeap };
	commands.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

CommandsMarker RenderCore::StartCommandList(CommandsData& commands, ID3D12DescriptorHeap* heap)
{
	// Reset command allocator and command list
	commands.commandAllocators[frameIndex]->Reset();
	commands.commandList->Reset(commands.commandAllocators[frameIndex], nullptr);

	ID3D12DescriptorHeap* descriptorHeaps[] = { heap };
	commands.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	return { commands };
}

void RenderCore::ExecuteCommandList(CommandsData& commands)
{
	// Close the command list and execute it
	commands.commandList->Close();

	ID3D12CommandList* commandLists[] = { commands.commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
}

HRESULT RenderCore::Present(bool vsync)
{
	return swapChain->Present(0, 0);
}

void RenderCore::EndFrame()
{
	MoveToNextFrame();
}

void RenderCore::WaitForAllFrames()
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

		fenceValues[i] += FrameCount;
	}
}

// Prepare to render the next frame.
void RenderCore::MoveToNextFrame()
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
		PIXBeginEvent(commandList, (UINT64)c.color, c.name.c_str());
	}
}

CommandsMarker::CommandsMarker(ID3D12GraphicsCommandList* c, const char* text, PixColor color) : commandList(c)
{
	PIXBeginEvent(commandList, (UINT64)color, text);
}

CommandsMarker::~CommandsMarker()
{
	close();
}

void CommandsMarker::move(const char* text, PixColor color)
{
	close();
	closed = false;
	PIXBeginEvent(commandList, (UINT64)color, text);
}

void CommandsMarker::mark(const char* text, PixColor color)
{
	if (!closed && commandList)
		PIXSetMarker(commandList, (UINT64)color, text);
}

void CommandsMarker::close()
{
	if (!closed && commandList)
	{
		closed = true;
		PIXEndEvent(commandList);
	}
}

GlobalQueueMarker::GlobalQueueMarker(ID3D12CommandQueue* q, const char* name) : queue(q)
{
	PIXBeginEvent(q, 0, name);
}

GlobalQueueMarker::~GlobalQueueMarker()
{
	PIXEndEvent(queue);
}
