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

	ID3D12DescriptorHeap* mainDescriptorHeap[2];

	void init(UINT maxDescriptors);

	void createTextureView(TextureResource& texture, UINT mipLevel = -1);
	void createTextureView(FileTexture& texture);
	void createTextureView(RenderTargetTexture& texture);
	void createTextureView(RenderTargetTexture& texture, UINT& descriptorOffset);
	void createDepthView(RenderDepthTargetTexture& texture);
	void createUAVView(TextureResource& texture);

	UINT nextDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;
	UINT previousDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;

	void removeTextureView(RenderTargetTexture& texture);
	void removeDepthView(RenderDepthTargetTexture& texture);

private:

	std::vector<D3D12_SRV_DIMENSION> descriptorTypes;

	UINT currentDescriptorCount = 0;
	ID3D12Device* device{};
};