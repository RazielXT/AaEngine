#pragma once

#include <d3d12.h>
#include "GpuTexture.h"

struct Texture;

class DescriptorManager
{
public:

	DescriptorManager(ID3D12Device& device);
	~DescriptorManager();

	static DescriptorManager& get();

	ID3D12DescriptorHeap* mainDescriptorHeap{};
	ID3D12DescriptorHeap* samplerHeap{};

	void init(UINT maxDescriptors);
	void initializeSamplers(float MipLODBias);

	void createTextureView(FileTexture& texture);
	void createTextureView(GpuTextureResource& texture, UINT mipLevels = -1);
	void createTextureView(GpuTexture3D& texture, UINT mipLevels = -1);
	void createTextureView(RenderTargetTextures& textures);
	void createUAVView(GpuTexture3D& texture);
	UINT createUAVView(GpuTextureResource& texture);
	std::vector<ShaderTextureViewUAV> createUAVMips(GpuTextureResource& texture);
	ShaderViewUAV createBufferView(ID3D12Resource* resource, UINT stride, UINT elements);

	UINT nextDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;
	UINT previousDescriptor(UINT offset, D3D12_SRV_DIMENSION) const;
	const char* getDescriptorName(UINT idx) const;

	void removeTextureView(RenderTargetTextures& textures);
	void removeTextureView(GpuTextureResource& texture);
	void removeUAV(GpuTextureResource& texture);
	void removeUAV(const ShaderViewUAV&);
	void removeUAV(const std::vector<ShaderTextureViewUAV>&);

private:

	struct DescriptorInfo
	{
		D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_UNKNOWN;
		const char* name{};
	};
	std::vector<DescriptorInfo> descriptorsInfo;

	ID3D12Device& device;

	void removeDescriptorIndex(UINT idx);
	UINT createDescriptorIndex(const DescriptorInfo&);
	UINT firstFreeDescriptor = 0;
	UINT currentDescriptorCount = 0;
};