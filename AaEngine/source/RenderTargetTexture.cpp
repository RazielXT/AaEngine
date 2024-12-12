#include "RenderTargetTexture.h"
#include "DescriptorManager.h"

static const float DepthClearValue = 0.f;
static const DirectX::XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

void RenderTargetHeap::InitRtv(ID3D12Device* device, UINT count, const wchar_t* name)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = count;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	rtvHeap->SetName(name ? name : L"RTV");
}

void RenderTargetHeap::InitDsv(ID3D12Device* device, UINT count, const wchar_t* name)
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = count;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	dsvHeap->SetName(name ? name : L"DSV");
}

void RenderTargetHeap::Reset()
{
	rtvHandlesCount = dsvHandlesCount = 0;
}

void RenderTargetHeap::CreateRenderTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle)
{
	rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), rtvHandlesCount, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	device->CreateRenderTargetView(texture.Get(), nullptr, rtvHandle);
	rtvHandlesCount++;
}

void RenderTargetHeap::CreateDepthTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle)
{
	dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), dsvHandlesCount, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	device->CreateDepthStencilView(texture.Get(), nullptr, dsvHandle);
	dsvHandlesCount++;
}

RenderTargetTexture::~RenderTargetTexture()
{
	DescriptorManager::get().removeTextureView(*this);
	DescriptorManager::get().removeUAVView(*this);
}

void RenderTargetTexture::Init(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, DXGI_FORMAT f, D3D12_RESOURCE_STATES state, UINT flags)
{
	width = w;
	height = h;
	format = f;

	CreateTextureBuffer(device, width, height, format, state, flags);

	if (flags & AllowRenderTarget)
		heap.CreateRenderTargetHandle(device, texture, view.handle);
}

void RenderTargetTexture::InitDepth(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState)
{
	width = w;
	height = h;

	CreateDepthBuffer(device, heap, initialState);
}

void RenderTargetTexture::InitExisting(ID3D12Resource* textureSource, ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, DXGI_FORMAT f)
{
	width = w;
	height = h;
	format = f;

	texture.Attach(textureSource);
	heap.CreateRenderTargetHandle(device, texture, view.handle);
}

void RenderTargetTexture::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			texture.Get(),
			from,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &barrier);
	}

	commandList->OMSetRenderTargets(1, &view.handle, TRUE, nullptr);

	if (clear)
		commandList->ClearRenderTargetView(view.handle, &clearColor.x, 0, nullptr);
}

void RenderTargetTexture::PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	if (from == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		return;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Get(),
		from,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);
}

void RenderTargetTexture::PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Get(),
		from,
		D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);
}

void RenderTargetTexture::CreateDepthBuffer(ID3D12Device* device, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState)
{
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
	clearValue.DepthStencil.Depth = DepthClearValue;
	clearValue.DepthStencil.Stencil = 0;

	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilResDesc,
			initialState,
			&clearValue,
			IID_PPV_ARGS(&texture)
		);

		heap.CreateDepthTargetHandle(device, texture, view.handle);
	}
}

void RenderTargetTexture::CreateTextureBuffer(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, UINT flags)
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
		IID_PPV_ARGS(&texture));
}

void RenderTargetTexture::PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clearDepth)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
			texture.Get(),
			from,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->ResourceBarrier(1, &barrierToRT);
	}

	commandList->OMSetRenderTargets(0, nullptr, FALSE, &view.handle);

	if (clearDepth)
		ClearDepth(commandList);
}

void RenderTargetTexture::ClearDepth(ID3D12GraphicsCommandList* commandList)
{
	commandList->ClearDepthStencilView(view.handle, D3D12_CLEAR_FLAG_DEPTH, DepthClearValue, 0, 0, nullptr);
}

void RenderTargetTexturesView::Init()
{
	auto t = texturesState.empty() ? depthState.texture : texturesState.front().texture;
	width = t->width;
	height = t->height;

	rtvHandles.clear();
	formats.clear();

	for (auto& t : texturesState)
	{
		rtvHandles.push_back(t.texture->view.handle);
		formats.push_back(t.texture->format);
	}
}

void RenderTargetTexturesView::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear, UINT flags)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	CD3DX12_RESOURCE_BARRIER barriers[5];
	size_t i = 0;
	{
		for (; i < texturesState.size(); i++)
		{
			barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
				texturesState[i].texture->texture.Get(),
				from,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		if (flags & TransitionFlags::UseDepth)
		{
			barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
				depthState.texture->texture.Get(),
				from,
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
	}
	commandList->ResourceBarrier(i, barriers);

	commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), TRUE, (flags & TransitionFlags::UseDepth) ? &depthState.texture->view.handle : nullptr);

	if (flags & TransitionFlags::UseDepth && flags & TransitionFlags::ClearDepth)
		commandList->ClearDepthStencilView(depthState.texture->view.handle, D3D12_CLEAR_FLAG_DEPTH, DepthClearValue, 0, 0, nullptr);

	if (clear)
	{
		for (size_t i = 0; i < texturesState.size(); i++)
			commandList->ClearRenderTargetView(rtvHandles[i], &clearColor.x, 0, nullptr);
	}
}

void RenderTargetTexturesView::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, bool clear, UINT flags)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	{
		CD3DX12_RESOURCE_BARRIER barriers[5];
		size_t i = 0;
		{
			for (auto& state : texturesState)
			{
				if (state.previousState != D3D12_RESOURCE_STATE_RENDER_TARGET)
				{
					barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
						state.texture->texture.Get(),
						state.previousState,
						D3D12_RESOURCE_STATE_RENDER_TARGET);
					i++;
				}
			}
			auto depthNextState = (flags & TransitionFlags::ReadOnlyDepth) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
			auto depthLastState = depthState.previousState;
			if ((flags & TransitionFlags::UseDepth) && !(flags & TransitionFlags::DepthSkipTransition) && depthLastState != depthNextState)
			{
				barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
					depthState.texture->texture.Get(),
					depthLastState,
					depthNextState);
			}
		}
		if (i)
			commandList->ResourceBarrier(i, barriers);
	}

	commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), TRUE, (flags & TransitionFlags::UseDepth) ? &depthState.texture->view.handle : nullptr);

	if ((flags & TransitionFlags::UseDepth) && (flags & TransitionFlags::ClearDepth))
		commandList->ClearDepthStencilView(depthState.texture->view.handle, D3D12_CLEAR_FLAG_DEPTH, DepthClearValue, 0, 0, nullptr);

	if (clear)
	{
		for (size_t i = 0; i < texturesState.size(); i++)
			commandList->ClearRenderTargetView(rtvHandles[i], &clearColor.x, 0, nullptr);
	}
}

void RenderTargetTexturesView::TransitionFromTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, bool useDepth /*= true*/)
{
	CD3DX12_RESOURCE_BARRIER barriers[5];
	size_t i = 0;
	{
		for (; i < texturesState.size(); i++)
		{
			barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
				texturesState[i].texture->texture.Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				to);
		}
		if (useDepth && depthState.texture)
		{
			barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
				depthState.texture->texture.Get(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				to);
		}
	}
	commandList->ResourceBarrier(i, barriers);
}

void RenderTargetTexturesView::PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
// 	if (from == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
// 		return;

	{
		CD3DX12_RESOURCE_BARRIER barriers[5];
		size_t i = 0;
		{
			for (; i < texturesState.size(); i++)
			{
				barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
					texturesState[i].texture->texture.Get(),
					from,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
// 			if (useDepth && depth.texture)
// 			{
// 				barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
// 					textures[i].texture.Get(),
// 					from,
// 					D3D12_RESOURCE_STATE_DEPTH_WRITE);
// 			}
		}
		commandList->ResourceBarrier(i, barriers);
	}

//	TransitionTarget(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, from);
}

void RenderTargetTexturesView::PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texturesState[0].texture->texture.Get(),
		from, D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);
}

void RenderTargetTexture::SetName(const std::string& n)
{
	name = n;

	if (texture)
		texture->SetName(std::wstring(name.begin(), name.end()).c_str());
}

void RenderTargetTextures::Init(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& formats, D3D12_RESOURCE_STATES initialState, bool depthBuffer, D3D12_RESOURCE_STATES initialDepthState)
{
	textures.reserve(formats.size());

	for (auto& f : formats)
	{
		textures.emplace_back().Init(device, w, h, heap, f, initialState);
		texturesState.push_back({ &textures.back() });
	}

	if (depthBuffer)
	{
		depth.InitDepth(device, w, h, heap, initialDepthState);
		depthState = { &depth };
	}

	RenderTargetTexturesView::Init();
}

void RenderTargetTextures::SetName(const std::string& name)
{
	for (auto& t : textures)
	{
		t.SetName(name);
	}

	depth.SetName(name + " Depth");
}
