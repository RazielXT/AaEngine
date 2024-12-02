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

	ID3D12DescriptorHeap* mainDescriptorHeap;

	void init(UINT maxDescriptors);

	void createTextureView(TextureResource& texture, UINT mipLevel = -1);
	void createTextureView(FileTexture& texture);
	void createTextureView(RenderTargetTexture& texture);
	void createTextureView(RenderTargetTexture& texture, UINT descriptorOffset);
	void createTextureView(RenderTargetTextures& textures);
	void createUAVView(TextureResource& texture);
	UINT createUAVView(RenderTargetTexture& texture, UINT mipLevel = 0);

	UINT nextDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;
	UINT previousDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;

	void removeTextureView(RenderTargetTextures& textures);
	void removeTextureView(RenderTargetTexture& texture);

private:

	std::vector<D3D12_SRV_DIMENSION> descriptorTypes;

	UINT currentDescriptorCount = 0;
	ID3D12Device* device{};
};