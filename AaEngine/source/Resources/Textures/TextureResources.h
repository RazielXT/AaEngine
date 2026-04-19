#pragma once

#include "Resources/ResourcesView.h"
#include <string>
#include <map>
#include <memory>

namespace DirectX
{
	class ResourceUploadBatch;
};

class FileTexture : public ShaderTextureView
{
public:

	ComPtr<ID3D12Resource> texture;

	void SetName(const std::string& name);
	std::string name;
};

struct TextureFileLoadOptions
{
	bool forceSrgb = false;
};

class TextureResources
{
public:

	TextureResources();
	~TextureResources();

	FileTexture* loadFile(ID3D12Device& device, DirectX::ResourceUploadBatch& batch, std::string file, TextureFileLoadOptions options = {});

	void setNamedTexture(std::string name, const ShaderTextureView& texture);
	ShaderTextureView* getNamedTexture(std::string name);

	void setNamedUAV(std::string name, const ShaderTextureViewUAV& texture);
	ShaderTextureViewUAV* getNamedUAV(std::string name);

private:

	std::map<std::string, std::unique_ptr<FileTexture>> loadedTextures;

	std::map<std::string, ShaderTextureView> namedTextures;
	std::map<std::string, ShaderTextureViewUAV> namedUAV;
};