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

private:

	std::map<std::string, std::unique_ptr<FileTexture>> loadedTextures;

	std::map<std::string, ShaderTextureView*> namedTextures;
};