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
	samplerHeap->Release();
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
		return;

	mainDescriptorHeap->SetName(L"CBV_SRV_UAV");
}

void DescriptorManager::createTextureView(FileTexture& texture)
{
	if (texture.srvHandles.ptr)
		return;

	auto desc = texture.texture->GetDesc();
	auto dimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	if (desc.DepthOrArraySize == 6)
		dimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

	auto index = createDescriptorIndex({ dimension, texture.name.c_str() });
	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), index
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = dimension;

	if (dimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
	{
		srvDesc.TextureCube.MipLevels = -1;
	}
	else
	{
		srvDesc.Texture2D.MipLevels = -1;
	}

	device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, handle);

	texture.srvHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), index
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	texture.srvHeapIndex = index;
}

void DescriptorManager::createTextureView(RenderTargetTexture& texture)
{
	if (texture.view.srvHandles.ptr)
		return;

	auto index = createDescriptorIndex({ D3D12_SRV_DIMENSION_TEXTURE2D, texture.name.c_str() });
	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), index
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

	texture.view.srvHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), index
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	texture.view.srvHeapIndex = index;
}

void DescriptorManager::createTextureView(TextureResource& texture, UINT mipLevel)
{
	if (texture.textureView.srvHandles.ptr)
		return;

	auto dimension = texture.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE3D;
	auto index = createDescriptorIndex({ dimension, texture.name.c_str() });
	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), index
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

	texture.textureView.srvHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), index
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	texture.textureView.srvHeapIndex = index;
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

		auto index = createDescriptorIndex({ D3D12_SRV_DIMENSION_BUFFER, texture.name.c_str() });
		texture.uav[mipLevel].uavCpuHandles = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), index
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, texture.uav[mipLevel].uavCpuHandles);

		texture.uav[mipLevel].uavHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), index
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		texture.uav[mipLevel].heapIndex = index;
	}
}

UINT DescriptorManager::createUAVView(RenderTargetTexture& texture, UINT mipLevel)
{
	if (texture.view.uavHeapIndex)
		return texture.view.uavHeapIndex;

	auto index = createDescriptorIndex({ D3D12_SRV_DIMENSION_BUFFER, texture.name.c_str() });

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

	auto uavCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), index
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, uavCpuHandle);

	texture.view.uavHandles = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), index
		, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	texture.view.uavHeapIndex = index;

	return index;
}

UINT DescriptorManager::nextDescriptor(UINT offset, D3D12_SRV_DIMENSION d) const
{
	offset = min(offset, descriptorsInfo.size());

	for (auto pos = offset + 1; descriptorsInfo.size() > pos; pos++)
		if (descriptorsInfo[pos].dimension == d)
			return pos;

	return offset;
}

UINT DescriptorManager::previousDescriptor(UINT offset, D3D12_SRV_DIMENSION d) const
{
	offset = min(offset, descriptorsInfo.size());

	for (auto pos = offset; pos > 0; pos--)
		if (descriptorsInfo[pos - 1].dimension == d)
			return pos - 1;

	return offset;
}

const char* DescriptorManager::getDescriptorName(UINT idx) const
{
	if (descriptorsInfo.size() <= idx)
		return nullptr;

	return descriptorsInfo[idx].name;
}

void DescriptorManager::removeTextureView(RenderTargetTexture& texture)
{
	removeDescriptorIndex(texture.view.srvHeapIndex);
}

void DescriptorManager::removeTextureView(RenderTargetTextures& rtt)
{
	removeTextureView(rtt.depth);

	for (auto& texture : rtt.textures | std::ranges::views::reverse)
	{
		removeTextureView(texture);
	}
}

void DescriptorManager::removeTextureView(TextureResource& texture)
{
	removeDescriptorIndex(texture.textureView.srvHeapIndex);
}

void DescriptorManager::removeUAVView(TextureResource& texture)
{
	for (auto& uav : texture.uav)
	{
		removeDescriptorIndex(uav.heapIndex);
	}
}

void DescriptorManager::removeUAVView(RenderTargetTexture& texture)
{
	removeDescriptorIndex(texture.view.uavHeapIndex);
}

void DescriptorManager::removeDescriptorIndex(UINT index)
{
	if (!index)
		return;

	descriptorsInfo[index] = {};
	firstFreeDescriptor = min(firstFreeDescriptor, index);

	while (index + 1 == currentDescriptorCount && descriptorsInfo[index].dimension == D3D12_SRV_DIMENSION_UNKNOWN)
	{
		currentDescriptorCount--;
		index--;
		descriptorsInfo.pop_back();
	}
}

UINT DescriptorManager::createDescriptorIndex(const DescriptorInfo& info)
{
	auto r = firstFreeDescriptor;

	if (firstFreeDescriptor == currentDescriptorCount)
	{
		descriptorsInfo.push_back(info);
		firstFreeDescriptor = ++currentDescriptorCount;
		return r;
	}

	descriptorsInfo[r] = info;

	for (UINT i = firstFreeDescriptor + 1; i <= currentDescriptorCount; i++)
	{
		if (i == currentDescriptorCount || descriptorsInfo[i].dimension == D3D12_SRV_DIMENSION_UNKNOWN)
		{
			firstFreeDescriptor = i;
			break;
		}
	}

	return r;
}

void DescriptorManager::initializeSamplers(float MipLODBias)
{
	if (!samplerHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.NumDescriptors = 2;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		auto hr = device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerHeap));
		if (FAILED(hr))
			return;

		samplerHeap->SetName(L"SAMPLERS");
	}

	int index = 0;
	//Anisotropic
	{
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = MipLODBias;
		samplerDesc.MaxAnisotropy = 8;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(samplerHeap->GetCPUDescriptorHandleForHeapStart(), index++, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));

		device->CreateSampler(&samplerDesc, handle);
	}
	//Linear
	{
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = MipLODBias;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(samplerHeap->GetCPUDescriptorHandleForHeapStart(), index++, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));

		device->CreateSampler(&samplerDesc, handle);
	}
}
