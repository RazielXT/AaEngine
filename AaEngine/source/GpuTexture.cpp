#include "GpuTexture.h"
#include "DescriptorManager.h"
#include "StringUtils.h"
#include <DirectXMath.h>

static const DirectX::XMFLOAT4 ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

void GpuTexture2D::Init(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, DXGI_FORMAT f, D3D12_RESOURCE_STATES state, UINT flags)
{
	width = w;
	height = h;
	format = f;

	CreateTextureBuffer(device, state, flags);

	if (flags & AllowRenderTarget)
		heap.CreateRenderTargetHandle(device, texture, view);
}

void GpuTexture2D::InitUAV(ID3D12Device* device, UINT w, UINT h, DXGI_FORMAT f, D3D12_RESOURCE_STATES state)
{
	width = w;
	height = h;
	format = f;

	CreateTextureBuffer(device, state, AllowUAV);
}

void GpuTexture2D::InitDepth(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, D3D12_RESOURCE_STATES initialState, float clearValue)
{
	width = w;
	height = h;
	format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue = clearValue;

	CreateDepthBuffer(device, initialState);

	heap.CreateDepthTargetHandle(device, texture, view.handle);
}

void GpuTexture2D::InitExisting(ID3D12Resource* textureSource, ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, DXGI_FORMAT f)
{
	width = w;
	height = h;
	format = f;

	texture.Attach(textureSource);
	heap.CreateRenderTargetHandle(device, texture, view);
}

void GpuTexture2D::PrepareAsRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_RENDER_TARGET)
		Transition(commandList, from, D3D12_RESOURCE_STATE_RENDER_TARGET);

	commandList->OMSetRenderTargets(1, &view.handle, TRUE, nullptr);

	if (clear)
		commandList->ClearRenderTargetView(view.handle, &ClearColor.x, 0, nullptr);
}

void GpuTexture2D::PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	if (from != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		Transition(commandList, from, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GpuTexture2D::PrepareToPresent(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	Transition(commandList, from, D3D12_RESOURCE_STATE_PRESENT);
}

void GpuTextureResource::Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
	if (from == to)
		return;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Get(),
		from,
		to);
	commandList->ResourceBarrier(1, &barrier);
}

void GpuTexture2D::CreateDepthBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_DESC depthStencilResDesc = {};
	depthStencilResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilResDesc.Alignment = 0;
	depthStencilResDesc.Width = width;
	depthStencilResDesc.Height = height;
	depthStencilResDesc.DepthOrArraySize = 1;
	depthStencilResDesc.MipLevels = 1;
	depthStencilResDesc.Format = format;
	depthStencilResDesc.SampleDesc.Count = 1;
	depthStencilResDesc.SampleDesc.Quality = 0;
	depthStencilResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	clearValue.DepthStencil.Depth = depthClearValue;
	clearValue.DepthStencil.Stencil = 0;

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilResDesc,
		initialState,
		&clearValue,
		IID_PPV_ARGS(&texture)
	);
}

void GpuTexture2D::CreateTextureBuffer(ID3D12Device* device, D3D12_RESOURCE_STATES state, UINT flags)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = depthOrArraySize;
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;
	memcpy(clearValue.Color, &ClearColor, sizeof(ClearColor));
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
		IID_PPV_ARGS(&texture)
	);
}

void GpuTexture2D::PrepareAsDepthTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clearDepth)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		Transition(commandList, from, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	commandList->OMSetRenderTargets(0, nullptr, FALSE, &view.handle);

	if (clearDepth)
		ClearDepth(commandList);
}

void GpuTexture2D::ClearDepth(ID3D12GraphicsCommandList* commandList)
{
	commandList->ClearDepthStencilView(view.handle, D3D12_CLEAR_FLAG_DEPTH, depthClearValue, 0, 0, nullptr);
}

void RenderTargetTexturesView::Init(ID3D12Device* device, std::vector<GpuTexture2D*> textures, GpuTexture2D* depthTexture)
{
	depth = depthTexture != nullptr;

	auto t = textures.empty() ? depthTexture : textures.front();
	width = t->width;
	height = t->height;

	rtvHandles.clear();
	formats.clear();

	for (auto& t : textures)
	{
		rtvHandles.push_back(t->view.handle);
		formats.push_back(t->format);
	}

	if (!rtvHandles.empty())
	{
		const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		const UINT64 base = rtvHandles[0].ptr;
		contiguous = true;

		for (size_t i = 1; i < rtvHandles.size(); ++i)
		{
			const UINT64 expected = base + static_cast<UINT64>(i) * inc;
			if (rtvHandles[i].ptr != expected)
				contiguous = false;
		}
	}
}

void RenderTargetTexturesView::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, const std::vector<GpuTextureStates>& states, bool clear, UINT flags)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	TextureTransitions<5>(states, commandList);

	auto depthTexture = ((flags & TransitionFlags::UseDepth) && depth) ? states.back().texture : nullptr;

	commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), contiguous, depthTexture ? &depthTexture->view.handle : nullptr);

	if (depthTexture && flags & TransitionFlags::ClearDepth)
		commandList->ClearDepthStencilView(depthTexture->view.handle, D3D12_CLEAR_FLAG_DEPTH, depthTexture->depthClearValue, 0, 0, nullptr);

	if (clear)
	{
		for (size_t i = 0; i < states.size() - depth; i++)
			commandList->ClearRenderTargetView(rtvHandles[i], &ClearColor.x, 0, nullptr);
	}
}

void GpuTextureStates::Transition(ID3D12GraphicsCommandList* commandList, const std::vector<GpuTextureStates>& states)
{
	CD3DX12_RESOURCE_BARRIER barriers[5];
	UINT i = 0;

	for (auto& s : states)
	{
		if (s.previousState != s.state)
			barriers[i++] = CD3DX12_RESOURCE_BARRIER::Transition(
				s.texture->texture.Get(),
				s.previousState,
				s.state);
	}

	if (i)
		commandList->ResourceBarrier(i, barriers);
}

void RenderTargetTextures::PrepareAsTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from, bool clear, UINT flags)
{
	auto vp = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList->RSSetViewports(1, &vp);
	auto rect = CD3DX12_RECT(0, 0, width, height);
	commandList->RSSetScissorRects(1, &rect);

	if (from != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		TextureTransitions<5> tr;
		for (auto& t : textures)
			tr.add(&t, from, D3D12_RESOURCE_STATE_RENDER_TARGET);

		auto depthNextState = (flags & TransitionFlags::ReadOnlyDepth) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if ((flags & TransitionFlags::UseDepth) && !(flags & TransitionFlags::DepthSkipTransition))
			tr.add(&depth, from, depthNextState);
		tr.push(commandList);
	}

	commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), contiguous, (flags & TransitionFlags::UseDepth) ? &depth.view.handle : nullptr);

	if (flags & TransitionFlags::UseDepth && flags & TransitionFlags::ClearDepth)
		commandList->ClearDepthStencilView(depth.view.handle, D3D12_CLEAR_FLAG_DEPTH, depth.depthClearValue, 0, 0, nullptr);

	if (clear)
	{
		for (size_t i = 0; i < textures.size(); i++)
			commandList->ClearRenderTargetView(rtvHandles[i], &ClearColor.x, 0, nullptr);
	}
}

void RenderTargetTextures::TransitionFromTarget(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to, bool useDepth /*= true*/)
{
	TextureTransitions<5> tr;
	for (auto& t : textures)
		tr.add(&t, D3D12_RESOURCE_STATE_RENDER_TARGET, to);
	if (useDepth && depth.texture)
		tr.add(&depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, to);
	tr.push(commandList);
}

void RenderTargetTextures::PrepareAsView(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES from)
{
	CD3DX12_RESOURCE_BARRIER barriers[5];
	size_t i = 0;
	{
		for (; i < textures.size(); i++)
		{
			barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
				textures[i].texture.Get(),
				from,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}
	commandList->ResourceBarrier(i, barriers);
}

void RenderTargetTextures::Init(ID3D12Device* device, UINT w, UINT h, RenderTargetHeap& heap, const std::vector<DXGI_FORMAT>& formats, D3D12_RESOURCE_STATES initialState, bool depthBuffer, D3D12_RESOURCE_STATES initialDepthState)
{
	std::vector<GpuTexture2D*> t;
	GpuTexture2D* d{};
	textures.reserve(formats.size());

	for (auto& f : formats)
	{
		textures.emplace_back().Init(device, w, h, heap, f, initialState);
		t.push_back(&textures.back());
	}

	if (depthBuffer)
	{
		depth.InitDepth(device, w, h, heap, initialDepthState);
		d = &depth;
	}

	RenderTargetTexturesView::Init(device, t, d);
}

void RenderTargetTextures::SetName(const std::string& name)
{
	for (auto& t : textures)
	{
		t.SetName(name);
	}

	depth.SetName(name + " Depth");
}

GpuTextureResource::~GpuTextureResource()
{
	DescriptorManager::get().removeTextureView(*this);
	DescriptorManager::get().removeUAVView(*this);
}

void GpuTextureResource::SetName(const std::string& n)
{
	name = n;

	if (texture)
		texture->SetName(as_wstring(name).c_str());
}

void GpuTexture3D::Init(ID3D12Device* device, UINT w, UINT h, UINT d, DXGI_FORMAT f)
{
	format = f;
	width = w;
	height = h;
	depthOrArraySize = d;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = depthOrArraySize;
	textureDesc.MipLevels = 0;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&texture));
}

void TextureStatePair::Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES to)
{
	if (currentState != to)
		texture->Transition(commandList, currentState, to);

	currentState = to;
}
