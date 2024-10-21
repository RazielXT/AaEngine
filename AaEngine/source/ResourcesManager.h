#pragma once

#include <d3d12.h>
#include "RenderTargetTexture.h"
#include "TextureResource.h"

struct Texture;

class ResourcesManager
{
public:

	ResourcesManager(ID3D12Device* device);
	~ResourcesManager();

	static ResourcesManager& get();

	ID3D12DescriptorHeap* mainDescriptorHeap[2];

	void init(UINT maxDescriptors);

	struct Cbuffer
	{
		ID3D12Resource* resource = nullptr;
		void* gpuAddress = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE viewHandle;
		UINT size = 0;
	};

	void createCbuffer(Cbuffer* bufferGPUAddress, UINT count, UINT size, void* defaultValue);

	void createCbufferView(Cbuffer* buffer, UINT count);
	void createShaderResourceView(TextureResource& texture, UINT mipLevel = -1);
	void createShaderResourceView(FileTexture& texture);
	void createShaderResourceView(RenderTargetTexture& texture);
	void createDepthShaderResourceView(RenderDepthTargetTexture& texture);
	void createUAVView(TextureResource& texture);

private:

	UINT currentDescriptorCount = 0;
	ID3D12Device* device{};
};