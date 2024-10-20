#pragma once

#include <d3d12.h>
#include "RenderTargetTexture.h"
#include "TextureResource.h"

struct Texture;

class ResourcesManager
{
public:

	ResourcesManager() = default;
	~ResourcesManager();

	ID3D12DescriptorHeap* mainDescriptorHeap[2]; // this heap will store the descripor to our constant buffer

	void init(ID3D12Device* device, UINT maxDescriptors);

	struct Cbuffer
	{
		ID3D12Resource* resource = nullptr;
		void* gpuAddress = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE viewHandle;
		UINT size = 0;
	};

	void createCbuffer(ID3D12Device* device, Cbuffer* bufferGPUAddress, UINT count, UINT size, void* defaultValue);

	void createCbufferView(ID3D12Device* device, Cbuffer* buffer, UINT count);
	void createShaderResourceView(ID3D12Device* device, TextureResource& texture);
	void createShaderResourceView(ID3D12Device* device, FileTexture& texture);
	void createShaderResourceView(ID3D12Device* device, RenderTargetTexture& texture);
	void createDepthShaderResourceView(ID3D12Device* device, RenderDepthTargetTexture& texture);
	void createUAVView(ID3D12Device* device, TextureResource& texture);

private:

	UINT currentDescriptorCount = 0;

};