#include "TextureResources.h"
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include "FileLogger.h"
#include "Directories.h"
#include "ResourceUploadBatch.h"
#include "StringUtils.h"

using namespace DirectX;

HRESULT LoadTextureFromFile(ResourceUploadBatch& resourceUpload, ID3D12Device& device, const std::string& filename, ID3D12Resource** texture)
{
	HRESULT hr;

	if (filename.ends_with("dds"))
	{
		hr = DirectX::CreateDDSTextureFromFileEx(
			&device,
			resourceUpload,
			as_wstring(filename).c_str(),
			0,
			D3D12_RESOURCE_FLAG_NONE,
			DDS_LOADER_FORCE_SRGB,
			texture);
	}
	else
	{
		hr = CreateWICTextureFromFileEx(
			&device,
			resourceUpload,
			as_wstring(filename).c_str(),
			0,
			D3D12_RESOURCE_FLAG_NONE,
			WIC_LOADER_MIP_AUTOGEN,
			texture);
	}

	if (FAILED(hr))
	{
		FileLogger::logErrorD3D("Failed to load texture from file: " + filename, hr);
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

FileTexture* TextureResources::loadFile(ID3D12Device& device, ResourceUploadBatch& resourceUpload, std::string file)
{
	if (file.front() != '.')
		file = TEXTURE_DIRECTORY + file;

	auto& t = loadedTextures[file];

	if (!t)
	{
		ID3D12Resource* resultTex = nullptr;
		if (LoadTextureFromFile(resourceUpload, device, file, &resultTex) == S_OK && resultTex)
		{
			t = std::make_unique<FileTexture>();
			t->texture = resultTex;
			t->SetName(file);
			resultTex->Release();
		}
	}

	return t.get();
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
