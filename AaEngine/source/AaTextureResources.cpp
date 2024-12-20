#include "AaTextureResources.h"
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include "AaLogger.h"
#include "Directories.h"
#include "ResourceUploadBatch.h"

using namespace DirectX;

HRESULT LoadTextureFromFile(ResourceUploadBatch& resourceUpload, ID3D12Device* device, const std::string& filename, ID3D12Resource** texture)
{
	HRESULT hr;

	if (filename.ends_with("dds"))
	{
		hr = DirectX::CreateDDSTextureFromFileEx(
			device,
			resourceUpload,
			std::wstring(filename.begin(), filename.end()).c_str(), -1, D3D12_RESOURCE_FLAG_NONE, DX12::DDS_LOADER_DEFAULT,
			texture);
	}
	else
	{
		hr = CreateWICTextureFromFile(
			device,
			resourceUpload,
			std::wstring(filename.begin(), filename.end()).c_str(),
			texture,
			true);
	}

	if (FAILED(hr))
	{
		AaLogger::logErrorD3D("Failed to load texture from file: " + filename, hr);
		return hr;
	}

	return hr;
}

static AaTextureResources* instance = nullptr;

AaTextureResources::AaTextureResources()
{
	if (instance)
		throw std::exception("Duplicate AaTextureResources");

	instance = this;
}

AaTextureResources::~AaTextureResources()
{
	instance = nullptr;
}

AaTextureResources& AaTextureResources::get()
{
	return *instance;
}

FileTexture* AaTextureResources::loadFile(ID3D12Device* device, ResourceUploadBatch& resourceUpload, std::string file)
{
	auto& t = loadedTextures[file];

	if (!t)
	{
		ID3D12Resource* resultTex = nullptr;
		if (LoadTextureFromFile(resourceUpload, device, TEXTURE_DIRECTORY + file, &resultTex) == S_OK && resultTex)
		{
			t = std::make_unique<FileTexture>();
			t->texture = resultTex;
			t->SetName(file);
			resultTex->Release();
		}
	}

	return t.get();
}

void AaTextureResources::setNamedTexture(std::string name, const ShaderTextureView& texture)
{
	namedTextures[name] = texture;
}

ShaderTextureView* AaTextureResources::getNamedTexture(std::string name)
{
	auto it = namedTextures.find(name);

	return it == namedTextures.end() ? nullptr : &it->second;
}

void AaTextureResources::setNamedUAV(std::string name, const ShaderUAV& texture)
{
	namedUAV[name] = texture;
}

ShaderUAV* AaTextureResources::getNamedUAV(std::string name)
{
	auto it = namedUAV.find(name);

	return it == namedUAV.end() ? nullptr : &it->second;
}

ShaderTextureView::ShaderTextureView(D3D12_GPU_DESCRIPTOR_HANDLE* handles)
{
	srvHandles = *handles;
}

ShaderUAV::ShaderUAV(D3D12_GPU_DESCRIPTOR_HANDLE* handles)
{
	uavHandles = *handles;
}

void FileTexture::SetName(const std::string& n)
{
	name = n;

	if (texture)
		texture->SetName(std::wstring(name.begin(), name.end()).c_str());
}
