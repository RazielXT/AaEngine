#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <tuple>

#ifdef _WIN32
#include <wincodec.h>
#include <wrl\client.h>
#else
#include <fstream>
#include <filesystem>
#include <thread>
#endif

#include "Directx.h"
#include "..\Src\d3dx12.h"

using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
inline void TransitionResource(
	_In_ ID3D12GraphicsCommandList* commandList,
	_In_ ID3D12Resource* resource,
	_In_ D3D12_RESOURCE_STATES stateBefore,
	_In_ D3D12_RESOURCE_STATES stateAfter) noexcept
{
	assert(commandList != nullptr);
	assert(resource != nullptr);

	if (stateBefore == stateAfter)
		return;

	D3D12_RESOURCE_BARRIER desc = {};
	desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	desc.Transition.pResource = resource;
	desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	desc.Transition.StateBefore = stateBefore;
	desc.Transition.StateAfter = stateAfter;

	commandList->ResourceBarrier(1, &desc);
}


//--------------------------------------------------------------------------------------
bool CaptureTexture(_In_ ID3D12Device* device,
	_In_ ID3D12CommandQueue* pCommandQ,
	_In_ ID3D12Resource* pSource,
	UINT64 srcPitch,
	const D3D12_RESOURCE_DESC& desc,
	_COM_Outptr_ ID3D12Resource** pStaging,
	D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState) noexcept
{
	if (pStaging)
	{
		*pStaging = nullptr;
	}

	if (!pCommandQ || !pSource || !pStaging)
		return false;

	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		return false;

	if (srcPitch > UINT32_MAX)
		return false;

	const UINT numberOfPlanes = D3D12GetFormatPlaneCount(device, desc.Format);
	if (numberOfPlanes != 1)
		return false;

	D3D12_HEAP_PROPERTIES sourceHeapProperties;
	HRESULT hr = pSource->GetHeapProperties(&sourceHeapProperties, nullptr);
	if (SUCCEEDED(hr) && sourceHeapProperties.Type == D3D12_HEAP_TYPE_READBACK)
	{
		// Handle case where the source is already a staging texture we can use directly
		*pStaging = pSource;
		pSource->AddRef();
		return true;
	}

	// Create a command allocator
	ComPtr<ID3D12CommandAllocator> commandAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_ID3D12CommandAllocator, reinterpret_cast<void**>(commandAlloc.GetAddressOf()));
	if (FAILED(hr))
		return false;

	// Spin up a new command list
	ComPtr<ID3D12GraphicsCommandList> commandList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAlloc.Get(), nullptr, IID_ID3D12GraphicsCommandList, reinterpret_cast<void**>(commandList.GetAddressOf()));
	if (FAILED(hr))
		return false;

	// Create a fence
	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, reinterpret_cast<void**>(fence.GetAddressOf()));
	if (FAILED(hr))
		return false;

	assert((srcPitch & 0xFF) == 0);

	const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);

	// Readback resources must be buffers
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.Height = 1;
	bufferDesc.Width = srcPitch * desc.Height;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.MipLevels = 1;
	bufferDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12Resource> copySource(pSource);
	D3D12_RESOURCE_STATES beforeStateSource = beforeState;
	if (desc.SampleDesc.Count > 1)
	{
		TransitionResource(commandList.Get(), pSource, beforeState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

		// MSAA content must be resolved before being copied to a staging texture
		auto descCopy = desc;
		descCopy.SampleDesc.Count = 1;
		descCopy.SampleDesc.Quality = 0;
		descCopy.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

		ComPtr<ID3D12Resource> pTemp;
		hr = device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&descCopy,
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			nullptr,
			IID_ID3D12Resource,
			reinterpret_cast<void**>(pTemp.GetAddressOf()));
		if (FAILED(hr))
			return hr;

		assert(pTemp);

		const DXGI_FORMAT fmt = desc.Format;

		D3D12_FEATURE_DATA_FORMAT_SUPPORT formatInfo = { fmt, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
		hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatInfo, sizeof(formatInfo));
		if (FAILED(hr))
			return hr;

		if (!(formatInfo.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D))
			return false;

		for (UINT item = 0; item < desc.DepthOrArraySize; ++item)
		{
			for (UINT level = 0; level < desc.MipLevels; ++level)
			{
				const UINT index = D3D12CalcSubresource(level, item, 0, desc.MipLevels, desc.DepthOrArraySize);
				commandList->ResolveSubresource(pTemp.Get(), index, pSource, index, fmt);
			}
		}

		copySource = pTemp;
		beforeState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
	}
	else
	{
		beforeStateSource = D3D12_RESOURCE_STATE_COPY_SOURCE;
	}

	// Create a staging texture
	hr = device->CreateCommittedResource(
		&readBackHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_ID3D12Resource,
		reinterpret_cast<void**>(pStaging));
	if (FAILED(hr))
		return false;

	// Transition the resource if necessary
	TransitionResource(commandList.Get(), copySource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_SOURCE);

	// Get the copy target location
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
	bufferFootprint.Footprint.Width = static_cast<UINT>(desc.Width);
	bufferFootprint.Footprint.Height = desc.Height;
	bufferFootprint.Footprint.Depth = 1;
	bufferFootprint.Footprint.RowPitch = static_cast<UINT>(srcPitch);
	bufferFootprint.Footprint.Format = desc.Format;

	const CD3DX12_TEXTURE_COPY_LOCATION copyDest(*pStaging, bufferFootprint);
	const CD3DX12_TEXTURE_COPY_LOCATION copySrc(copySource.Get(), 0);

	// Copy the texture
	commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);

	// Transition the source resource to the next state
	TransitionResource(commandList.Get(), pSource, beforeStateSource, afterState);

	hr = commandList->Close();
	if (FAILED(hr))
	{
		(*pStaging)->Release();
		*pStaging = nullptr;
		return false;
	}

	// Execute the command list
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	pCommandQ->ExecuteCommandLists(1, commandLists);

	// Signal the fence
	hr = pCommandQ->Signal(fence.Get(), 1);
	if (FAILED(hr))
	{
		(*pStaging)->Release();
		*pStaging = nullptr;
		return false;
	}

	// Block until the copy is complete
	while (fence->GetCompletedValue() < 1)
	{
		SwitchToThread();
	}

	return true;
}

bool SaveTextureToMemory(
	ID3D12CommandQueue* pCommandQ,
	ID3D12Resource* pSource,
	std::vector<UCHAR>& data,
	D3D12_RESOURCE_STATES beforeState,
	D3D12_RESOURCE_STATES afterState)
{
	ComPtr<ID3D12Device> device;
	pCommandQ->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(device.GetAddressOf()));

	// Get the size of the image
	auto const desc = pSource->GetDesc();

	if (desc.Width > UINT32_MAX)
		return false;

	UINT64 totalResourceSize = 0;
	UINT64 fpRowPitch = 0;
	UINT fpRowCount = 0;
	// Get the rowcount, pitch and size of the top mip
	device->GetCopyableFootprints(
		&desc,
		0,
		1,
		0,
		nullptr,
		&fpRowCount,
		&fpRowPitch,
		&totalResourceSize);

	// Round up the srcPitch to multiples of 256
	const UINT64 dstRowPitch = (fpRowPitch + 255) & ~0xFFu;

	if (dstRowPitch > UINT32_MAX)
		return false;

	ComPtr<ID3D12Resource> pStaging;
	HRESULT hr = CaptureTexture(device.Get(), pCommandQ, pSource, dstRowPitch, desc, pStaging.GetAddressOf(), beforeState, afterState);
	if (FAILED(hr))
		return false;

	const UINT64 imageSize = dstRowPitch * UINT64(desc.Height);
	if (imageSize > UINT32_MAX)
		return false;

	void* pMappedMemory = nullptr;
	D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(imageSize) };
	D3D12_RANGE writeRange = { 0, 0 };
	hr = pStaging->Map(0, &readRange, &pMappedMemory);
	if (FAILED(hr))
		return false;

	data.resize(totalResourceSize);
	memcpy(data.data(), pMappedMemory, totalResourceSize);

	pStaging->Unmap(0, &writeRange);

	if (FAILED(hr))
		return false;

	return true;
}

#include <vector>
#include <fstream>
#include <cstdint>

#pragma pack(push, 1)
struct BMPHeader {
	uint16_t fileType{ 0x4D42 };           // "BM"
	uint32_t fileSize{ 0 };                // Size of the file (in bytes)
	uint16_t reserved1{ 0 };               // Reserved, must be zero
	uint16_t reserved2{ 0 };               // Reserved, must be zero
	uint32_t offsetData{ 54 };             // Start position of pixel data
};

struct DIBHeader {
	uint32_t size{ 40 };                   // Size of this header (in bytes)
	int32_t width{ 0 };                    // Width of the bitmap (in pixels)
	int32_t height{ 0 };                   // Height of the bitmap (in pixels)
	uint16_t planes{ 1 };                  // Number of color planes (must be 1)
	uint16_t bitCount{ 32 };               // Number of bits per pixel
	uint32_t compression{ 0 };             // Compression type (0 for none)
	uint32_t imageSize{ 0 };               // Size of image data (in bytes)
	int32_t xPixelsPerMeter{ 0 };          // Horizontal resolution (pixels per meter)
	int32_t yPixelsPerMeter{ 0 };          // Vertical resolution (pixels per meter)
	uint32_t colorsUsed{ 0 };              // Number of colors in the color palette
	uint32_t importantColors{ 0 };         // Number of important colors
};
#pragma pack(pop)

void ConvertRGBAToBGRA(std::vector<UCHAR>& imageData)
{
	for (size_t i = 0; i < imageData.size(); i += 4) {
		std::swap(imageData[i], imageData[i + 2]); // Swap R and B 
	}
}

void FlipVertical(std::vector<UCHAR>& imageData, int width, int height)
{
	int rowSize = width * 4; // Assuming 4 bytes per pixel (RGBA)
	std::vector<UCHAR> tempRow(rowSize);

	for (int y = 0; y < height / 2; ++y) {
		int topIdx = y * rowSize;
		int bottomIdx = (height - y - 1) * rowSize;

		// Swap rows
		memcpy(tempRow.data(), &imageData[topIdx], rowSize);
		memcpy(&imageData[topIdx], &imageData[bottomIdx], rowSize);
		memcpy(&imageData[bottomIdx], tempRow.data(), rowSize);
	}
}

void SaveBMP(std::vector<UCHAR>& imageData, UINT width, UINT height, const char* filename)
{
	ConvertRGBAToBGRA(imageData);
	FlipVertical(imageData, width, height);

	BMPHeader bmpHeader;
	DIBHeader dibHeader;

	dibHeader.width = width;
	dibHeader.height = height;
	bmpHeader.fileSize = sizeof(BMPHeader) + sizeof(DIBHeader) + imageData.size();

	std::ofstream file(filename, std::ios::out | std::ios::binary);
	if (!file) {
		throw std::runtime_error("Unable to open file");
	}

	file.write(reinterpret_cast<const char*>(&bmpHeader), sizeof(bmpHeader));
	file.write(reinterpret_cast<const char*>(&dibHeader), sizeof(dibHeader));
	file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());

	file.close();
}
