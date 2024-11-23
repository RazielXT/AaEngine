#include "RenderTargetTexture.h"

void RenderDepthTargetTexture::Init(ID3D12Device* device, UINT w, UINT h, D3D12_RESOURCE_STATES initialState, UINT arraySize)
{
	width = w;
	height = h;

	CreateDepthBuffer(device, initialState, arraySize);
}

void RenderDepthTargetTexture::ClearDepth(ID3D12GraphicsCommandList* commandList)
{
	commandList->ClearDepthStencilView(dsvHandles, D3D12_CLEAR_FLAG_DEPTH, depthClearValue, 0, 0, nullptr);
}

void RenderTargetHeap::Init(ID3D12Device* device, UINT count, const wchar_t* name)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = count;
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

void RenderTargetTexture::Init(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& f, D3D12_RESOURCE_STATES state, bool depthBuffer, D3D12_RESOURCE_STATES initialDepthState)
{
	width = w;
	height = h;
	formats = f;

	for (auto format : f)
	{
		auto& t = textures.emplace_back();
		CreateTextureBuffer(device, width, height, t, format, state);
	}

	if (flags & AllowRenderTarget)
	{
		rtvHandles.resize(textures.size());
		UINT c = 0;

		for (auto& t : textures)
		{
			heap.CreateRenderTargetHandle(device, t.texture, rtvHandles[c++]);
		}
	}

	if (depthBuffer)
		RenderDepthTargetTexture::Init(device, width, height, initialDepthState);
}

void RenderTargetTexture::InitExisting(ID3D12Resource* textureSource, ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, DXGI_FORMAT f)
{
	width = w;
	height = h;
	formats = { f };

	auto& t = textures.emplace_back();
	t.texture.Attach(textureSource);
	heap.CreateRenderTargetHandle(device, t.texture, rtvHandles.emplace_back());

	RenderDepthTargetTexture::Init(device, width, height);
}

void RenderDepthTargetTexture::CreateDepthBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES initialState, UINT arraySize)
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
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
	clearValue.DepthStencil.Depth = depthClearValue;
	clearValue.DepthStencil.Stencil = 0;

	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilResDesc,
			initialState,
			&clearValue,
			IID_PPV_ARGS(&depthStencilTexture)
		);

		dsvHandles = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), 0, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

		if (arraySize > 1)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = arraySize;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.MipSlice = 0;
			device->CreateDepthStencilView(depthStencilTexture.Get(), &dsvDesc, dsvHandles);
		}
		else
			device->CreateDepthStencilView(depthStencilTexture.Get(), nullptr, dsvHandles);
	}
}

void RenderTargetTexture::CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, Texture& t, DXGI_FORMAT format, D3D12_RESOURCE_STATES state)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = arraySize;
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	memcpy(clearValue.Color, &clearColor, sizeof(clearColor));
	auto clearValuePtr = &clearValue;

	if (flags & AllowRenderTarget)
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (flags & AllowUAV)
	{
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		clearValuePtr = nullptr;
	}

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		state,
		clearValuePtr,
		IID_PPV_ARGS(&t.texture));
}

void RenderDepthTargetTexture::PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
			depthStencilTexture.Get(),
			from,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->ResourceBarrier(1, &barrierToRT);
	}

	commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandles);
	commandList->ClearDepthStencilView(dsvHandles, D3D12_CLEAR_FLAG_DEPTH, depthClearValue, 0, 0, nullptr);
}

void RenderDepthTargetTexture::PrepareAsDepthView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	if (from != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		TransitionDepth(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, from);
	}
}

void RenderDepthTargetTexture::TransitionDepth(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, D3D12_RESOURCE_STATES from)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		depthStencilTexture.Get(),
		from,
		to);
	commandList->ResourceBarrier(1, &barrier);
}

void RenderTargetTexture::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear, bool depth, bool clearDepth)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		CD3DX12_RESOURCE_BARRIER barriers[4];
		{
			for (size_t i = 0; i < textures.size(); i++)
			{
				barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
					textures[i].texture.Get(),
					from,
					D3D12_RESOURCE_STATE_RENDER_TARGET);
			}

			commandList->ResourceBarrier(textures.size(), barriers);
		}
	}

	commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), TRUE, (depth && dsvHeap) ? &dsvHandles : nullptr);

	if (depth && clearDepth)
		commandList->ClearDepthStencilView(dsvHandles, D3D12_CLEAR_FLAG_DEPTH, depthClearValue, 0, 0, nullptr);

	if (clear)
	{
		for (size_t i = 0; i < textures.size(); i++)
			commandList->ClearRenderTargetView(rtvHandles[i], &clearColor.x, 0, nullptr);
	}
}

void RenderTargetTexture::PrepareAsSingleTarget(ID3D12GraphicsCommandList* commandList, UINT textureIndex, D3D12_RESOURCE_STATES from, bool clear /*= true*/, bool depth /*= true*/, bool clearDepth /*= true*/)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		CD3DX12_RESOURCE_BARRIER barriers = CD3DX12_RESOURCE_BARRIER::Transition(
			textures[textureIndex].texture.Get(),
			from,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList->ResourceBarrier(1, &barriers);
	}

	commandList->OMSetRenderTargets(1, &rtvHandles[textureIndex], TRUE, (depth && dsvHeap) ? &dsvHandles : nullptr);

	if (depth && clearDepth)
		commandList->ClearDepthStencilView(dsvHandles, D3D12_CLEAR_FLAG_DEPTH, depthClearValue, 0, 0, nullptr);

	if (clear)
	{
		commandList->ClearRenderTargetView(rtvHandles[textureIndex], &clearColor.x, 0, nullptr);
	}
}

void RenderTargetTexture::TransitionTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, D3D12_RESOURCE_STATES from)
{
	CD3DX12_RESOURCE_BARRIER barriers[4];
	for (size_t i = 0; i < textures.size(); i++)
	{
		barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
			textures[i].texture.Get(),
			from,
			to);
	}

	commandList->ResourceBarrier(textures.size(), barriers);
}

void RenderTargetTexture::PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	if (from == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		return;

	TransitionTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, from);
}

void RenderTargetTexture::PrepareAsSingleView(ID3D12GraphicsCommandList* commandList, UINT textureIndex, D3D12_RESOURCE_STATES from)
{
	if (from == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		return;

	CD3DX12_RESOURCE_BARRIER barriers = CD3DX12_RESOURCE_BARRIER::Transition(
		textures[textureIndex].texture.Get(),
		from,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->ResourceBarrier(1, &barriers);
}

void RenderTargetTexture::PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		textures[0].texture.Get(),
		from, D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);
}

void RenderTargetTexture::SetName(const wchar_t* name)
{
	for (auto& t : textures)
	{
		if (t.texture)
			t.texture->SetName(name);
	}

	RenderDepthTargetTexture::SetName((std::wstring(name) + L" Depth").c_str());
}

void RenderDepthTargetTexture::SetName(const wchar_t* name)
{
	if (depthStencilTexture)
		depthStencilTexture->SetName(name);
}

ShaderTextureView& RenderTargetInfo::textureView(UINT i)
{
	return textures[i].textureView;
}

UINT RenderTargetInfo::srvHeapIndex() const
{
	return textures.front().textureView.srvHeapIndex;
}

UINT RenderTargetInfo::uavHeapIndex() const
{
	return textures.front().textureView.uavHeapIndex;
}
