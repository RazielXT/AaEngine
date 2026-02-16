#include "RenderTargetHeap.h"

void RenderTargetHeap::InitRtv(ID3D12Device* device, UINT count, const wchar_t* name)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = count;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	rtvHeap->SetName(name ? name : L"RTV");
	Reset();
}

void RenderTargetHeap::InitDsv(ID3D12Device* device, UINT count, const wchar_t* name)
{
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = count;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	dsvHeap->SetName(name ? name : L"DSV");
	Reset();
}

void RenderTargetHeap::Reset()
{
	rtvHandlesCount = dsvHandlesCount = 0;
}

void RenderTargetHeap::CreateRenderTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, ShaderTextureView& view, DXGI_FORMAT f)
{
	if (view.handle.ptr)
	{
		view.handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), view.heapIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	}
	else
	{
		view.handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), rtvHandlesCount, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		view.heapIndex = rtvHandlesCount;
		rtvHandlesCount++;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {}; 
	rtvDesc.Format = f;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	device->CreateRenderTargetView(texture.Get(), f ? &rtvDesc : nullptr, view.handle);
}

void RenderTargetHeap::CreateDepthTargetHandle(ID3D12Device* device, ComPtr<ID3D12Resource>& texture, ShaderTextureView& view)
{
	if (view.handle.ptr)
	{
		view.handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), view.heapIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	}
	else if (!dsvFreeHandles.empty())
	{
		view.heapIndex = dsvFreeHandles.back();
		dsvFreeHandles.pop_back();
		view.handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), view.heapIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	}
	else
	{
		view.handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvHeap->GetCPUDescriptorHandleForHeapStart(), dsvHandlesCount, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
		view.heapIndex = dsvHandlesCount;
		dsvHandlesCount++;
	}

	device->CreateDepthStencilView(texture.Get(), nullptr, view.handle);
}

void RenderTargetHeap::RemoveDepthTargetHandle(ShaderTextureView& view)
{
	if (view.handle.ptr)
		dsvFreeHandles.push_back(view.heapIndex);
}
