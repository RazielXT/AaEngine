#include "DescriptorManager.h"
#include "directx\d3dx12.h"
#include "AaTextureResources.h"
#include <ranges>

static DescriptorManager* instance = nullptr;

DescriptorManager::DescriptorManager(ID3D12Device* d) : device(d)
{
	if (instance)
		throw std::exception("Duplicate ResourcesManager");

	instance = this;
}

DescriptorManager::~DescriptorManager()
{
	mainDescriptorHeap->Release();

	instance = nullptr;
}

DescriptorManager& DescriptorManager::get()
{
	return *instance;
}

void DescriptorManager::init(UINT maxDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = maxDescriptors;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap));
	if (FAILED(hr))
	{
		// Handle error
		return;
	}

	mainDescriptorHeap->SetName(L"CBV_SRV_UAV");
}

void DescriptorManager::createTextureView(FileTexture& texture)
{
	if (texture.srvHandles.ptr)
		return;

	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texture.texture->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = -1;
	device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, handle);

	texture.srvHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	texture.srvHeapIndex = currentDescriptorCount;

	descriptorTypes.push_back(D3D12_SRV_DIMENSION_TEXTURE2D);
	currentDescriptorCount++;
}

void DescriptorManager::createTextureView(RenderTargetTexture& texture, UINT descriptorOffset)
{
	if (texture.view.srvHandles.ptr)
		return;

	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), descriptorOffset
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (texture.arraySize > 1)
	{
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2DArray.ArraySize = texture.arraySize;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	}
	else
	{
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	}

	srvDesc.Format = texture.texture->GetDesc().Format;
	if (srvDesc.Format == DXGI_FORMAT_D32_FLOAT)
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;

	device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, handle);

	texture.view.srvHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorOffset
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	texture.view.srvHeapIndex = descriptorOffset;
}

void DescriptorManager::createTextureView(RenderTargetTexture& texture)
{
	createTextureView(texture, currentDescriptorCount);

	descriptorTypes.push_back(D3D12_SRV_DIMENSION_TEXTURE2D);
	currentDescriptorCount++;
}

void DescriptorManager::createTextureView(TextureResource& texture, UINT mipLevel)
{
	if (texture.textureView.srvHandles.ptr)
		return;

	auto dimension = texture.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE3D;
	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = dimension;
	if (mipLevel != -1)
	{
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = mipLevel;
	}
	else
		srvDesc.Texture2D.MipLevels = -1;
	srvDesc.Format = texture.texture->GetDesc().Format;
	device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, handle);

	texture.textureView.srvHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	texture.textureView.srvHeapIndex = currentDescriptorCount;

	descriptorTypes.push_back(dimension);
	currentDescriptorCount++;
}

void DescriptorManager::createTextureView(RenderTargetTextures& rtt)
{
	for (auto& t : rtt.textures)
	{
		createTextureView(t);
	}

	createTextureView(rtt.depth);
}

void DescriptorManager::createUAVView(TextureResource& texture)
{
	auto mipLevels = texture.texture->GetDesc().MipLevels;
	texture.uav.resize(mipLevels);

	for (UINT mipLevel = 0; mipLevel < mipLevels; mipLevel++)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texture.format;
		uavDesc.ViewDimension = texture.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? D3D12_UAV_DIMENSION_TEXTURE3D : D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture3D.MipSlice = mipLevel;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = -1;

		texture.uav[mipLevel].uavCpuHandles = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, texture.uav[mipLevel].uavCpuHandles);

		texture.uav[mipLevel].uavHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		descriptorTypes.push_back(D3D12_SRV_DIMENSION_BUFFER);
		currentDescriptorCount++;
	}
}

UINT DescriptorManager::createUAVView(RenderTargetTexture& texture, UINT mipLevel)
{
	if (texture.view.uavHeapIndex)
		return texture.view.uavHeapIndex;

	auto firstDescriptor = currentDescriptorCount;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = texture.arraySize == 1 ? D3D12_UAV_DIMENSION_TEXTURE2D : D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = texture.format;

	if (texture.arraySize == 1)
		uavDesc.Texture2D.MipSlice = mipLevel;
	else
	{
		uavDesc.Texture2DArray.MipSlice = mipLevel;
		uavDesc.Texture2DArray.ArraySize = texture.arraySize;
	}

	auto uavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, uavCpuHandle);

	texture.view.uavHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	descriptorTypes.push_back(D3D12_SRV_DIMENSION_BUFFER);
	texture.view.uavHeapIndex = currentDescriptorCount;
	currentDescriptorCount++;

	return firstDescriptor;
}

UINT DescriptorManager::nextDescriptor(UINT offset, D3D12_SRV_DIMENSION d) const
{
	offset = min(offset, descriptorTypes.size());

	for (auto pos = offset + 1; descriptorTypes.size() > pos; pos++)
		if (descriptorTypes[pos] == d)
			return pos;

	return offset;
}

UINT DescriptorManager::previousDescriptor(UINT offset, D3D12_SRV_DIMENSION d) const
{
	offset = min(offset, descriptorTypes.size());

	for (auto pos = offset; pos > 0; pos--)
		if (descriptorTypes[pos - 1] == d)
			return pos - 1;

	return offset;
}

void DescriptorManager::removeTextureView(RenderTargetTexture& texture)
{
	auto index = texture.view.srvHeapIndex;

	if (index + 1 == currentDescriptorCount)
	{
		currentDescriptorCount--;
		descriptorTypes.pop_back();
	}
	else
		descriptorTypes[index] = D3D12_SRV_DIMENSION_UNKNOWN;
}

void DescriptorManager::removeTextureView(RenderTargetTextures& rtt)
{
	removeTextureView(rtt.depth);

	for (auto& texture : rtt.textures | std::ranges::views::reverse)
	{
		removeTextureView(texture);
	}
}
