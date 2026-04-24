#include "Resources/Textures/TextureResources.h"
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include "Utils/Logger.h"
#include "App/Directories.h"
#include "ResourceUploadBatch.h"
#include "Utils/StringUtils.h"

using namespace DirectX;

HRESULT LoadTextureFromFile(ResourceUploadBatch& resourceUpload, ID3D12Device& device, const std::string& filename, ID3D12Resource** texture, TextureFileLoadOptions options)
{
	HRESULT hr;

	if (filename.ends_with("dds"))
	{
		auto flags = DDS_LOADER_DEFAULT;
		if (options.forceSrgb)
			flags |= DDS_LOADER_FORCE_SRGB;

		hr = DirectX::CreateDDSTextureFromFileEx(
			&device,
			resourceUpload,
			as_wstring(filename).c_str(),
			0,
			D3D12_RESOURCE_FLAG_NONE,
			flags,
			texture);
	}
	else
	{
		auto flags = WIC_LOADER_MIP_AUTOGEN;
		if (options.forceSrgb)
			flags |= WIC_LOADER_FORCE_SRGB;
		else
			flags |= WIC_LOADER_IGNORE_SRGB;

		hr = CreateWICTextureFromFileEx(
			&device,
			resourceUpload,
			as_wstring(filename).c_str(),
			0,
			D3D12_RESOURCE_FLAG_NONE,
			flags,
			texture);
	}

	if (FAILED(hr))
	{
		Logger::logErrorD3D("Failed to load texture from file: " + filename, hr);
		return hr;
	}

	return hr;
}

TextureResources::TextureResources()
{
}

TextureResources::~TextureResources()
{
}

FileTexture* TextureResources::loadFile(ID3D12Device& device, ResourceUploadBatch& resourceUpload, std::string file, TextureFileLoadOptions options)
{
	if (file.front() != '.')
		file = TEXTURE_DIRECTORY + file;

	auto& t = loadedTextures[file];

	if (!t)
	{
		ID3D12Resource* resultTex = nullptr;
		if (LoadTextureFromFile(resourceUpload, device, file, &resultTex, options) == S_OK && resultTex)
		{
			t = std::make_unique<FileTexture>();
			t->texture = resultTex;
			t->SetName(file);
			resultTex->Release();
		}
	}

	return t.get();
}

FileTexture* TextureResources::getFile(std::string file)
{
	if (file.front() != '.')
		file = TEXTURE_DIRECTORY + file;

	return loadedTextures[file].get();
}

void TextureResources::setNamedTexture(std::string name, const ShaderTextureView& texture)
{
	namedTextures[name] = texture;
}

ShaderTextureView* TextureResources::getNamedTexture(std::string name)
{
	auto it = namedTextures.find(name);

	return it == namedTextures.end() ? nullptr : &it->second;
}

void TextureResources::setNamedUAV(std::string name, const ShaderTextureViewUAV& texture)
{
	namedUAV[name] = texture;
}

ShaderTextureViewUAV* TextureResources::getNamedUAV(std::string name)
{
	auto it = namedUAV.find(name);

	return it == namedUAV.end() ? nullptr : &it->second;
}

void FileTexture::SetName(const std::string& n)
{
	name = n;

	if (texture)
		texture->SetName(as_wstring(name).c_str());
}
