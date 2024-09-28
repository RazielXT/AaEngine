#include "RenderTargetTexture.h"
#include "directx\d3dx12.h"

void RenderDepthTargetTexture::Init(ID3D12Device* device, UINT width, UINT height, int frameCount)
{
	CreateDepthBuffer(device, width, height, frameCount);
	dsvLastState[0] = dsvLastState[1] = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void RenderTargetTexture::Init(ID3D12Device* device, UINT width, UINT height, int frameCount, DXGI_FORMAT f)
{
	format = f;
	CreateTextureBuffer(device, width, height, frameCount);
	CreateTextureBufferHandles(device, frameCount);
	rtvLastState[0] = rtvLastState[1] = D3D12_RESOURCE_STATE_COMMON;

	RenderDepthTargetTexture::Init(device, width, height, frameCount);
}

void RenderTargetTexture::InitExisting(ID3D12Resource** textureSource, ID3D12Device* device, UINT width, UINT height, int frameCount, DXGI_FORMAT f)
{
	format = f;

	for (int i = 0; i < frameCount; i++)
	{
		texture[i].Attach(textureSource[i]);
	}
	CreateTextureBufferHandles(device, frameCount);
	rtvLastState[0] = rtvLastState[1] = D3D12_RESOURCE_STATE_PRESENT;

	RenderDepthTargetTexture::Init(device, width, height, frameCount);
}

void RenderDepthTargetTexture::CreateDepthBuffer(ID3D12Device* device, UINT width, UINT height, int frameCount)
{
	this->width = width;
	this->height = height;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = frameCount;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	dsvHeap->SetName(L"DSV");

	D3D12_RESOURCE_DESC depthStencilResDesc = {};
	depthStencilResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilResDesc.Alignment = 0;
	depthStencilResDesc.Width = width;
	depthStencilResDesc.Height = height;
	depthStencilResDesc.DepthOrArraySize = 1;
	depthStencilResDesc.MipLevels = 1;
	depthStencilResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilResDesc.SampleDesc.Count = 1;
	depthStencilResDesc.SampleDesc.Quality = 0;
	depthStencilResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	for (UINT n = 0; n < frameCount; n++)
	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilResDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&depthStencilTexture[n])
		);

		dsvHandles[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), n, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
		device->CreateDepthStencilView(depthStencilTexture[n].Get(), nullptr, dsvHandles[n]);
	}
}

void RenderTargetTexture::CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, int frameCount)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	memcpy(clearValue.Color, &clearColor, sizeof(clearColor));

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	for (int i = 0; i < 2; ++i)
	{
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&clearValue,
			IID_PPV_ARGS(&texture[i]));
	}
}

void RenderTargetTexture::CreateTextureBufferHandles(ID3D12Device* device, int frameCount)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = frameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	rtvHeap->SetName(L"RTV");

	for (int i = 0; i < frameCount; ++i)
	{
		rtvHandles[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), i, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		device->CreateRenderTargetView(texture[i].Get(), nullptr, rtvHandles[i]);
	}
}

void RenderDepthTargetTexture::PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (dsvLastState[frameIndex] != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
			depthStencilTexture[frameIndex].Get(),
			dsvLastState[frameIndex],
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->ResourceBarrier(1, &barrierToRT);

		dsvLastState[frameIndex] = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandles[frameIndex]);
	//commandList->ClearRenderTargetView(rtvHandles[frameIndex], &clearColor.x, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandles[frameIndex], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void RenderDepthTargetTexture::PrepareAsDepthView(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		depthStencilTexture[frameIndex].Get(),
		dsvLastState[frameIndex],
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	dsvLastState[frameIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void RenderTargetTexture::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, int frameIndex, bool clear)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (rtvLastState[frameIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
			texture[frameIndex].Get(),
			rtvLastState[frameIndex],
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &barrierToRT);

		rtvLastState[frameIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	commandList->OMSetRenderTargets(1, &rtvHandles[frameIndex], FALSE, &dsvHandles[frameIndex]);

	if (clear)
		commandList->ClearRenderTargetView(rtvHandles[frameIndex], &clearColor.x, 0, nullptr);
	//commandList->ClearDepthStencilView(dsvHandles[frameIndex], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void RenderTargetTexture::PrepareAsView(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture[frameIndex].Get(),
		rtvLastState[frameIndex],
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	rtvLastState[frameIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void RenderTargetTexture::PrepareToPresent(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture[frameIndex].Get(),
		rtvLastState[frameIndex], D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);

	rtvLastState[frameIndex] = D3D12_RESOURCE_STATE_PRESENT;
}
