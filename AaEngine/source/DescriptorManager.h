#pragma once

#include <d3d12.h>
#include "RenderTargetTexture.h"
#include "TextureResource.h"

struct Texture;

class DescriptorManager
{
public:

	DescriptorManager(ID3D12Device* device);
	~DescriptorManager();

	static DescriptorManager& get();

	ID3D12DescriptorHeap* mainDescriptorHeap{};
	ID3D12DescriptorHeap* samplerHeap{};

	void init(UINT maxDescriptors);
	void initializeSamplers(float MipLODBias);

	void createTextureView(TextureResource& texture, UINT mipLevel = -1);
	void createTextureView(FileTexture& texture);
	void createTextureView(RenderTargetTexture& texture);
	void createTextureView(RenderTargetTextures& textures);
	void createUAVView(TextureResource& texture);
	UINT createUAVView(RenderTargetTexture& texture, UINT mipLevel = 0);

	UINT nextDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;
	UINT previousDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;

	void removeDescriptorIndex(UINT idx);
	void removeTextureView(RenderTargetTextures& textures);
	void removeTextureView(RenderTargetTexture& texture);
	void removeTextureView(TextureResource& texture);
	void removeUAVView(RenderTargetTexture& texture);
	void removeUAVView(TextureResource& texture);
private:

	std::vector<D3D12_SRV_DIMENSION> descriptorTypes;

	ID3D12Device* device{};

	UINT createDescriptorIndex(D3D12_SRV_DIMENSION);
	UINT firstFreeDescriptor = 0;
	UINT currentDescriptorCount = 0;
};