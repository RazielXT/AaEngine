#pragma once

#include "Directx.h"
#include <string>
#include <map>
#include <memory>

namespace DirectX
{
	class ResourceUploadBatch;
};

struct ShaderTextureView
{
	ShaderTextureView() = default;
	ShaderTextureView(D3D12_GPU_DESCRIPTOR_HANDLE*);

	D3D12_CPU_DESCRIPTOR_HANDLE handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandles{};
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandles{};
	UINT rtvHeapIndex{};
	UINT srvHeapIndex{};
	UINT uavHeapIndex{};
};

class ShaderUAV
{
public:

	ShaderUAV() = default;
	ShaderUAV(D3D12_GPU_DESCRIPTOR_HANDLE*);

	D3D12_GPU_DESCRIPTOR_HANDLE uavHandles{};
	D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandles{};
	UINT mipLevel{};
	UINT heapIndex{};
};

class FileTexture : public ShaderTextureView
{
public:

	ComPtr<ID3D12Resource> texture;

	void SetName(const std::string& name);
	std::string name;
};

class TextureResources
{
public:

	TextureResources();
	~TextureResources();

	FileTexture* loadFile(ID3D12Device& device, DirectX::ResourceUploadBatch& batch, std::string file);

	void setNamedTexture(std::string name, const ShaderTextureView& texture);
	ShaderTextureView* getNamedTexture(std::string name);

	void setNamedUAV(std::string name, const ShaderUAV& texture);
	ShaderUAV* getNamedUAV(std::string name);

private:

	std::map<std::string, std::unique_ptr<FileTexture>> loadedTextures;

	std::map<std::string, ShaderTextureView> namedTextures;
	std::map<std::string, ShaderUAV> namedUAV;
};