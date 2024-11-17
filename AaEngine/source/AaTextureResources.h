#pragma once

#include "Directx.h"
#include <string>
#include <map>
#include <memory>

namespace DirectX
{
	class ResourceUploadBatch;
};

class ShaderTextureView
{
public:

	ShaderTextureView() = default;
	ShaderTextureView(D3D12_GPU_DESCRIPTOR_HANDLE*);

	D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[2]{};
	UINT srvHeapIndex{};
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandles[2]{};
	UINT uavHeapIndex{};
};

class ShaderUAV
{
public:

	ShaderUAV() = default;
	ShaderUAV(D3D12_GPU_DESCRIPTOR_HANDLE*);

	D3D12_GPU_DESCRIPTOR_HANDLE uavHandles[2]{};
	D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandles[2]{};
	UINT mipLevel{};
};

class FileTexture : public ShaderTextureView
{
public:

	ComPtr<ID3D12Resource> texture;
};

class AaTextureResources
{
public:

	AaTextureResources();
	~AaTextureResources();

	static AaTextureResources& get();

	FileTexture* loadFile(ID3D12Device* device, DirectX::ResourceUploadBatch& batch, std::string file);

	void setNamedTexture(std::string name, ShaderTextureView* texture);
	ShaderTextureView* getNamedTexture(std::string name);

	void setNamedUAV(std::string name, ShaderUAV* texture);
	ShaderUAV* getNamedUAV(std::string name);

private:

	std::map<std::string, std::unique_ptr<FileTexture>> loadedTextures;

	std::map<std::string, ShaderTextureView*> namedTextures;
	std::map<std::string, ShaderUAV*> namedUAV;
};