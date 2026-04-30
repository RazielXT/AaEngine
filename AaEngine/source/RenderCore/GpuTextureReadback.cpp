#include "RenderCore/GpuTextureReadback.h"
#include "directx/d3dx12.h"

void GpuTextureReadback::init(ID3D12Device* device, UINT w, UINT h, DXGI_FORMAT fmt)
{
	width = w;
	height = h;
	format = fmt;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = format;
	texDesc.SampleDesc.Count = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT64 totalBytes{};
	device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);
	rowPitch = footprint.Footprint.RowPitch;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = totalBytes;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackBuffer));
}

void GpuTextureReadback::copyFrom(ID3D12GraphicsCommandList* commandList, ID3D12Resource* srcTexture)
{
	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = srcTexture;
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
	dstLocation.pResource = readbackBuffer.Get();
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstLocation.PlacedFootprint.Offset = 0;
	dstLocation.PlacedFootprint.Footprint.Format = format;
	dstLocation.PlacedFootprint.Footprint.Width = width;
	dstLocation.PlacedFootprint.Footprint.Height = height;
	dstLocation.PlacedFootprint.Footprint.Depth = 1;
	dstLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;

	commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
}

const void* GpuTextureReadback::map()
{
	void* data = nullptr;
	D3D12_RANGE readRange = { 0, (SIZE_T)(rowPitch * height) };
	readbackBuffer->Map(0, &readRange, &data);
	return data;
}

void GpuTextureReadback::unmap()
{
	D3D12_RANGE writeRange = { 0, 0 };
	readbackBuffer->Unmap(0, &writeRange);
}

UINT GpuTextureReadback::getRowPitch() const
{
	return rowPitch;
}

UINT GpuTextureReadback::getWidth() const
{
	return width;
}

UINT GpuTextureReadback::getHeight() const
{
	return height;
}

bool GpuTextureReadback::isInitialized() const
{
	return readbackBuffer != nullptr;
}
