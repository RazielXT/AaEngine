#include "RenderTargetTexture.h"
#include "directx\d3dx12.h"

void RenderDepthTargetTexture::Init(ID3D12Device* device, UINT w, UINT h, UINT frameCount, UINT arraySize)
{
	width = w;
	height = h;

	CreateDepthBuffer(device, frameCount, arraySize);
}

void RenderTargetHeap::Init(ID3D12Device* device, UINT count, UINT frameCount, const wchar_t* name)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = count * frameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	rtvHeap->SetName(name ? name : L"RTV");
}

void RenderTargetHeap::Reset()
{
	handlesCount = 0;
}

void RenderTargetHeap::CreateRenderTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle)
{
	rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), handlesCount, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	device->CreateRenderTargetView(texture.Get(), nullptr, rtvHandle);
	handlesCount++;
}

void RenderTargetTexture::Init(ID3D12Device* device, UINT w, UINT h, UINT frameCount, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& f, bool depthBuffer)
{
	width = w;
	height = h;
	formats = f;

	for (auto format : f)
	{
		auto& t = textures.emplace_back();
		CreateTextureBuffer(device, width, height, frameCount, t, format);
	}

	for (int i = 0; i < frameCount; i++)
	{
		rtvHandles[i].resize(textures.size());
		UINT c = 0;

		for (auto& t : textures)
		{
			heap.CreateRenderTargetHandle(device, t.texture[i], rtvHandles[i][c++]);
		}
	}

	if (depthBuffer)
		RenderDepthTargetTexture::Init(device, width, height, frameCount);
}

void RenderTargetTexture::InitExisting(ID3D12Resource** textureSource, ID3D12Device* device, UINT w, UINT h, UINT frameCount, RenderTargetHeap& heap, DXGI_FORMAT f)
{
	width = w;
	height = h;
	formats = { f };

	auto& t = textures.emplace_back();

	for (int i = 0; i < frameCount; i++)
	{
		t.texture[i].Attach(textureSource[i]);
		rtvLastState[i] = D3D12_RESOURCE_STATE_PRESENT;

		heap.CreateRenderTargetHandle(device, t.texture[i], rtvHandles[i].emplace_back());
	}

	RenderDepthTargetTexture::Init(device, width, height, frameCount);
}

void RenderDepthTargetTexture::CreateDepthBuffer(ID3D12Device* device, UINT frameCount, UINT arraySize)
{
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
	depthStencilResDesc.DepthOrArraySize = arraySize;
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
		dsvLastState[n] = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		if (arraySize > 1)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = arraySize;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;
			device->CreateDepthStencilView(depthStencilTexture[n].Get(), &dsvDesc, dsvHandles[n]);
		}
		else
			device->CreateDepthStencilView(depthStencilTexture[n].Get(), nullptr, dsvHandles[n]);
	}
}

void RenderTargetTexture::CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, UINT frameCount, Texture& t, DXGI_FORMAT format)
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

	for (int i = 0; i < frameCount; ++i)
	{
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&clearValue,
			IID_PPV_ARGS(&t.texture[i]));
	}
}

void RenderDepthTargetTexture::PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
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

void RenderDepthTargetTexture::PrepareAsDepthView(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		depthStencilTexture[frameIndex].Get(),
		dsvLastState[frameIndex],
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	dsvLastState[frameIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void RenderTargetTexture::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, UINT frameIndex, bool clear, bool depth, bool clearDepth)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	CD3DX12_RESOURCE_BARRIER barriers[4];
	if (rtvLastState[frameIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		for (size_t i = 0; i < textures.size(); i++)
		{	
			barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
				textures[i].texture[frameIndex].Get(),
				rtvLastState[frameIndex],
				D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		commandList->ResourceBarrier(textures.size(), barriers);
		rtvLastState[frameIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	commandList->OMSetRenderTargets(rtvHandles[frameIndex].size(), rtvHandles[frameIndex].data(), TRUE, (depth && dsvHeap) ? &dsvHandles[frameIndex] : nullptr);

	if (depth && clearDepth)
		commandList->ClearDepthStencilView(dsvHandles[frameIndex], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	if (clear)
	{
		for (size_t i = 0; i < textures.size(); i++)
			commandList->ClearRenderTargetView(rtvHandles[frameIndex][i], &clearColor.x, 0, nullptr);
	}
}

void RenderTargetTexture::PrepareAsView(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	CD3DX12_RESOURCE_BARRIER barriers[4];
	for (size_t i = 0; i < textures.size(); i++)
	{
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
			textures[i].texture[frameIndex].Get(),
			rtvLastState[frameIndex],
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	commandList->ResourceBarrier(textures.size(), barriers);
	rtvLastState[frameIndex] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void RenderTargetTexture::PrepareToPresent(ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		textures[0].texture[frameIndex].Get(),
		rtvLastState[frameIndex], D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);

	rtvLastState[frameIndex] = D3D12_RESOURCE_STATE_PRESENT;
}

void RenderTargetTexture::SetName(const wchar_t* name)
{
	for (auto& t : textures)
	{
		for (auto& r : t.texture)
		{
			if (r)
				r->SetName(name);
		}
	}

	RenderDepthTargetTexture::SetName(name);
}

void RenderDepthTargetTexture::SetName(const wchar_t* name)
{
	if (dsvHeap)
	{
		for (auto& t : depthStencilTexture)
		{
			if (t)
				t->SetName(name);
		}
	}
}
