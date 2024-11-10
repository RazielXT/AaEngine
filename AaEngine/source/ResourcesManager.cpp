#include "ResourcesManager.h"
#include "directx\d3dx12.h"
#include "AaTextureResources.h"

static ResourcesManager* instance = nullptr;

ResourcesManager::ResourcesManager(ID3D12Device* d) : device(d)
{
	if (instance)
		throw std::exception("Duplicate ResourcesManager");

	instance = this;
}

ResourcesManager::~ResourcesManager()
{
	for (auto h : mainDescriptorHeap)
	{
		h->Release();
	}

	instance = nullptr;
}

ResourcesManager& ResourcesManager::get()
{
	return *instance;
}

void ResourcesManager::init(UINT maxDescriptors)
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

void ResourcesManager::createCbuffer(Cbuffer* buffer, UINT count, UINT bufferSize, void* defaultValue)
{
	// create the constant buffer resource heap
	// We will update the constant buffer one or more times per frame, so we will use only an upload heap
	// unlike previously we used an upload heap to upload the vertex and index data, and then copied over
	// to a default heap. If you plan to use a resource for more than a couple frames, it is usually more
	// efficient to copy to a default heap where it stays on the gpu. In this case, our constant buffer
	// will be modified and uploaded at least once per frame, so we only use an upload heap

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	UINT size = 1024 * 64;
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

	// create a resource heap, descriptor heap, and pointer to cbv for each frame
	for (int i = 0; i < count; ++i)
	{
		auto hr = device->CreateCommittedResource(
			&heapProp, // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&resDesc, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&buffer[i].resource));

		buffer[i].resource->SetName(L"Constant Buffer Upload Resource");

		CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
		hr = buffer[i].resource->Map(0, &readRange, reinterpret_cast<void**>(&buffer[i].gpuAddress));
		buffer[i].size = bufferSize;
	}

// 	D3D12_HEAP_PROPERTIES heapProps = {};
// 	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
// 
// 	auto reservedbufferSize = 1024 * 64;
// 	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(reservedbufferSize);
// 
// 	for (int i = 0; i < 2; ++i)
// 	{
// 		device->CreateCommittedResource(
// 			&heapProps,
// 			D3D12_HEAP_FLAG_NONE,
// 			&resDesc,
// 			D3D12_RESOURCE_STATE_GENERIC_READ,
// 			nullptr,
// 			IID_PPV_ARGS(&constantBufferResource[i]));
// 
// 
// 		UINT8* pCbvDataBegin;
// 		D3D12_RANGE readRange = {}; // We do not intend to read from this resource on the CPU.
// 		constantBufferResource[i]->Map(0, &readRange, reinterpret_cast<void**>(&pCbvDataBegin));
// 		memcpy(pCbvDataBegin, &defaultValue, bufferSize);
// 		constantBufferResource[i]->Unmap(0, nullptr);
// 	}
}

void ResourcesManager::createCbufferView(Cbuffer* buffer, UINT count)
{
	for (int i = 0; i < count; ++i)
	{
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = buffer[i].resource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (buffer[i].size + 255) & ~255;    // CB size is required to be 256-byte aligned.
		device->CreateConstantBufferView(&cbvDesc, handle);

		buffer[i].viewHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mainDescriptorHeap[i]->GetGPUDescriptorHandleForHeapStart(), currentDescriptorCount
			, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	}

	descriptorTypes.push_back(D3D12_SRV_DIMENSION_BUFFER);
	currentDescriptorCount++;
}

void ResourcesManager::createShaderResourceView(FileTexture& texture)
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

void ResourcesManager::createShaderResourceView(RenderTargetTexture& rtt, UINT& descriptorOffset)
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

void ResourcesManager::createShaderResourceView(RenderTargetTexture& texture)
{
	auto orig = currentDescriptorCount;
	createShaderResourceView(texture, currentDescriptorCount);

	for (orig; orig < currentDescriptorCount; orig++)
	{
		descriptorTypes.push_back(D3D12_SRV_DIMENSION_TEXTURE2D);
	}
}

void ResourcesManager::createShaderResourceView(TextureResource& texture, UINT mipLevel)
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

void ResourcesManager::createDepthShaderResourceView(RenderDepthTargetTexture& texture)
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

void ResourcesManager::createUAVView(TextureResource& texture)
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

UINT ResourcesManager::nextDescriptor(UINT offset, D3D12_SRV_DIMENSION d) const
{
	offset = min(offset, descriptorTypes.size());

	for (auto pos = offset + 1; descriptorTypes.size() > pos; pos++)
		if (descriptorTypes[pos] == d)
			return pos;

	return offset;
}

UINT ResourcesManager::previousDescriptor(UINT offset, D3D12_SRV_DIMENSION d) const
{
	offset = min(offset, descriptorTypes.size());

	for (auto pos = offset; pos > 0; pos--)
		if (descriptorTypes[pos - 1] == d)
			return pos - 1;

	return offset;
}
