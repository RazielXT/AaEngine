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
		hr = DirectX::CreateDDSTextureFromFile(
			device,
			resourceUpload,
			std::wstring(filename.begin(), filename.end()).c_str(),
			texture,
			true);
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
			resultTex->Release();
		}
	}

	return t.get();
}

void AaTextureResources::setNamedTexture(std::string name, ShaderTextureView* texture)
{
	namedTextures[name] = texture;
}

ShaderTextureView* AaTextureResources::getNamedTexture(std::string name)
{
	auto it = namedTextures.find(name);

	return it == namedTextures.end() ? nullptr : it->second;
}

void AaTextureResources::setNamedUAV(std::string name, ShaderUAV* texture)
{
	namedUAV[name] = texture;
}

ShaderUAV* AaTextureResources::getNamedUAV(std::string name)
{
	auto it = namedUAV.find(name);

	return it == namedUAV.end() ? nullptr : it->second;
}

ShaderTextureView::ShaderTextureView(D3D12_GPU_DESCRIPTOR_HANDLE* handles)
{
	for (int i = 0; i < _countof(srvHandles); i++)
	{
		srvHandles[i] = handles[i];
	}
}

ShaderUAV::ShaderUAV(D3D12_GPU_DESCRIPTOR_HANDLE* handles)
{
	for (int i = 0; i < _countof(uavHandles); i++)
	{
		uavHandles[i] = handles[i];
	}
}
