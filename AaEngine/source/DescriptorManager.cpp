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
	for (auto h : mainDescriptorHeap)
	{
		h->Release();
	}

	instance = nullptr;
}

DescriptorManager& DescriptorManager::get()
{
	return *instance;
}

void DescriptorManager::init(UINT maxDescriptors)
{
	for (int i = 0; i < 2; ++i)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = maxDescriptors;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap[i]));
		if (FAILED(hr))
		{
			// Handle error
			return;
		}

		mainDescriptorHeap[i]->SetName(L"CBV_SRV_UAV");
	}
}

void DescriptorManager::createTextureView(FileTexture& texture)
{
	for (int i = 0; i < 2; ++i)
	{
		if (texture.srvHandles[i].ptr)
			return;

		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture.texture->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = -1;
		device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, handle);

		texture.srvHandles[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		texture.srvHeapIndex = currentDescriptorCount;
	}

	descriptorTypes.push_back(D3D12_SRV_DIMENSION_TEXTURE2D);
	currentDescriptorCount++;
}

void DescriptorManager::createTextureView(RenderTargetTexture& rtt, UINT& descriptorOffset)
{
	for (auto& texture : rtt.textures)
	{
		for (int i = 0; i < 2; ++i)
		{
			if (texture.textureView.srvHandles[i].ptr)
				return;

			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart(), descriptorOffset
				, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			//if single frame resource, view same resource from both frames
			auto& target = texture.texture[i] ? texture.texture[i] : texture.texture[0];

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Format = target->GetDesc().Format;
			device->CreateShaderResourceView(target.Get(), &srvDesc, handle);

			texture.textureView.srvHandles[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetGPUDescriptorHandleForHeapStart(), descriptorOffset
				, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		}

		texture.textureView.srvHeapIndex = descriptorOffset;
		descriptorOffset++;
	}
}

void DescriptorManager::createTextureView(RenderTargetTexture& texture)
{
	auto orig = currentDescriptorCount;
	createTextureView(texture, currentDescriptorCount);

	for (orig; orig < currentDescriptorCount; orig++)
	{
		descriptorTypes.push_back(D3D12_SRV_DIMENSION_TEXTURE2D);
	}
}

void DescriptorManager::createTextureView(TextureResource& texture, UINT mipLevel)
{
	auto dimension = texture.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE3D;

	for (int i = 0; i < 2; ++i)
	{
		if (texture.textureView.srvHandles[i].ptr)
			return;

		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
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
		srvDesc.Format = texture.textures[i]->GetDesc().Format;
		device->CreateShaderResourceView(texture.textures[i].Get(), &srvDesc, handle);

		texture.textureView.srvHandles[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		texture.textureView.srvHeapIndex = currentDescriptorCount;
	}

	descriptorTypes.push_back(dimension);
	currentDescriptorCount++;
}

void DescriptorManager::createDepthView(RenderDepthTargetTexture& texture)
{
	for (int i = 0; i < 2; ++i)
	{
		if (texture.depthView.srvHandles[i].ptr)
			return;

		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		auto& target = texture.depthStencilTexture[i] ? texture.depthStencilTexture[i] : texture.depthStencilTexture[0];

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		device->CreateShaderResourceView(target.Get(), &srvDesc, handle);

		texture.depthView.srvHandles[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		texture.depthView.srvHeapIndex = currentDescriptorCount;
	}

	descriptorTypes.push_back(D3D12_SRV_DIMENSION_TEXTURE2D);
	currentDescriptorCount++;
}

void DescriptorManager::createUAVView(TextureResource& texture)
{
	auto mipLevels = texture.textures[0]->GetDesc().MipLevels;
	texture.uav.resize(mipLevels);

	for (UINT mipLevel = 0; mipLevel < mipLevels; mipLevel++)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texture.format;
		uavDesc.ViewDimension = texture.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? D3D12_UAV_DIMENSION_TEXTURE3D : D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture3D.MipSlice = mipLevel;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = -1;

		for (int i = 0; i < 2; ++i)
		{
			texture.uav[mipLevel].uavCpuHandles[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
				, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			device->CreateUnorderedAccessView(texture.textures[i].Get(), nullptr, &uavDesc, texture.uav[mipLevel].uavCpuHandles[i]);

			texture.uav[mipLevel].uavHandles[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
				, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		}

		descriptorTypes.push_back(D3D12_SRV_DIMENSION_BUFFER);
		currentDescriptorCount++;
	}
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

void DescriptorManager::removeTextureView(RenderTargetTexture& rtt)
{
	for (auto& texture : rtt.textures | std::ranges::views::reverse)
	{
		auto index = texture.textureView.srvHeapIndex;

		if (index + 1 == currentDescriptorCount)
		{
			currentDescriptorCount--;
			descriptorTypes.pop_back();
		}
		else
			descriptorTypes[index] = D3D12_SRV_DIMENSION_UNKNOWN;
	}
}

void DescriptorManager::removeDepthView(RenderDepthTargetTexture& texture)
{
	auto index = texture.depthView.srvHeapIndex;

	if (index + 1 == currentDescriptorCount)
	{
		currentDescriptorCount--;
		descriptorTypes.pop_back();
	}
	else
		descriptorTypes[index] = D3D12_SRV_DIMENSION_UNKNOWN;
}
