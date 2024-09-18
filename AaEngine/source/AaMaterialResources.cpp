#include "AaMaterialResources.h"
#include "AaLogger.h"
#include "AaMaterialFileParser.h"
#include "Directories.h"
#include <DirectXTex.h>
#include "AaShaderManager.h"

HRESULT LoadTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filename, ID3D11ShaderResourceView** textureView, TexMetadata* metadataOut = nullptr)
{
	// Load the image using DirectXTex
	TexMetadata metadata{};
	ScratchImage scratchImg;
	HRESULT hr = LoadFromWICFile(std::wstring(filename.begin(), filename.end()).c_str(), WIC_FLAGS_NONE, &metadata, scratchImg);

	if (FAILED(hr))
	{
		AaLogger::logErrorD3D("Failed to load texture from file: " + filename, hr);
		return hr;
	}

	// Create the texture
	hr = CreateShaderResourceView(device, scratchImg.GetImages(), scratchImg.GetImageCount(), metadata, textureView);
	if (FAILED(hr))
	{
		AaLogger::logErrorD3D("Failed to create shader resource view from texture: " + filename, hr);
		return hr;
	}

	if (metadataOut)
		*metadataOut = metadata;

	return S_OK;
}

AaMaterialResources* instance = nullptr;

AaMaterialResources& AaMaterialResources::get()
{
	return *instance;
}

AaMaterialResources::AaMaterialResources(AaRenderSystem* mRenderSystem)
{
	if (instance)
		throw std::exception("Duplicate AaMaterialResources");

	instance = this;

	this->mRenderSystem = mRenderSystem;

	std::string filePosition = TEXTURE_DIRECTORY + "Default.png";

	HRESULT d3dResult = LoadTextureFromFile(mRenderSystem->getDevice(), mRenderSystem->getContext(), filePosition, &defaultTexture);

	if (FAILED(d3dResult))
		AaLogger::logError("Failed to load the default texture image");
	else
		AaLogger::log("Loaded texture image Default.png");

	//defaultFiltering
	D3D11_SAMPLER_DESC textureMapDesc;
	ZeroMemory(&textureMapDesc, sizeof(textureMapDesc));
	textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	textureMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	textureMapDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	textureMapDesc.MaxAnisotropy = 8;
	textureMapDesc.MaxLOD = D3D11_FLOAT32_MAX;
	d3dResult = mRenderSystem->getDevice()->CreateSamplerState(&textureMapDesc, &defaultMapSampler);

	if (FAILED(d3dResult))
		AaLogger::logError("Failed to create default sampler state");
}

AaMaterialResources::~AaMaterialResources()
{
	for (auto createdSampler : createdSamplers)
	{
		createdSampler->Release();
	}

	for (auto it : loadedTextures)
	{
		it.second->Release();
	}

	for (auto it : loadedUAVs)
	{
		if (it.second)
			it.second->Release();
	}

	for (auto& it : materialMap)
		delete it.second;

	defaultTexture->Release();
	defaultMapSampler->Release();

	instance = nullptr;
}

AaMaterial* AaMaterialResources::getMaterial(std::string name)
{
	auto it = materialMap.find(name);

	if (it == materialMap.end())
		return defaultMaterial;

	return it->second;
}

void AaMaterialResources::addUAV(ID3D11UnorderedAccessView* view, std::string name)
{
	loadedUAVs[name] = view;

	for (auto& m : materialMap)
	{
		m.second->updateUAV(name, view);
	}
}

ID3D11SamplerState* AaMaterialResources::getSampler(const textureInfo& info)
{
	if (info.defaultSampler)
		return defaultMapSampler;

	ID3D11SamplerState* targetSampler;
	//defaultFiltering
	D3D11_SAMPLER_DESC textureMapDesc;
	ZeroMemory(&textureMapDesc, sizeof(textureMapDesc));

	if (info.bordering == TextureBorder::Clamp)
	{
		textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	}
	else if (info.bordering == TextureBorder::BorderColor)
	{
		textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		textureMapDesc.BorderColor[0] = info.border_color[0];
		textureMapDesc.BorderColor[1] = info.border_color[1];
		textureMapDesc.BorderColor[2] = info.border_color[2];
		textureMapDesc.BorderColor[3] = info.border_color[3];
	}
	else
	{
		textureMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		textureMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		textureMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	}

	textureMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	if (info.filter == Filtering::None)
	{
		textureMapDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	}
	else if (info.filter == Filtering::Bilinear)
	{
		textureMapDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		textureMapDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		textureMapDesc.MaxAnisotropy = info.maxAnisotropy;
	}

	textureMapDesc.MaxLOD = D3D11_FLOAT32_MAX;
	mRenderSystem->getDevice()->CreateSamplerState(&textureMapDesc, &targetSampler);

	createdSamplers.push_back(targetSampler);

	return targetSampler;
}

void AaMaterialResources::loadMaterials(std::string directory, bool subDirectories)
{
	std::vector<materialInfo> matInfos = AaMaterialFileParser::parseAllMaterialFiles(directory, subDirectories);

	for (const materialInfo& info : matInfos)
	{
		if (materialMap.contains(info.name))
		{
			AaLogger::logWarning("skip loading duplicate material " + info.name);
			continue;
		}

		auto mat = materialMap[info.name] = new AaMaterial(info.name, mRenderSystem);
		mat->mRenderState = mRenderSystem->getRenderState(info.renderStateDesc);

		//set shader programs loaded from shd file
		if (!info.vs_ref.empty())
		{
			auto sh = AaShaderManager::get().getVertexShader(info.vs_ref);
			if (!sh)
			{
				AaLogger::logError("vertex program " + info.vs_ref + " for material " + info.name + " not found");
			}
			else
			{
				mat->setShader(sh, ShaderTypeVertex, info.defaultParams);

				for (const auto& vstexture : info.vstextures)
				{
					ID3D11ShaderResourceView* tex = nullptr;
					if (!vstexture.file.empty())
						tex = getTextureFileResource(vstexture.file);
					else if (!vstexture.name.empty())
						tex = getTextureNamedResource(vstexture.name);

					mat->addTexture(tex, getSampler(vstexture), vstexture.name, ShaderTypeVertex);
				}
			}
		}

		if (!info.ps_ref.empty())
		{
			auto sh = AaShaderManager::get().getPixelShader(info.ps_ref);
			if (!sh)
			{
				AaLogger::logError("pixel program " + info.ps_ref + " for material " + info.name + " not found");
			}
			else
			{
				mat->setShader(sh, ShaderTypePixel, info.defaultParams);

				//pixel shader textures
				for (auto& pstexture : info.pstextures)
				{
					ID3D11ShaderResourceView* tex = nullptr;
					if (!pstexture.file.empty())
						tex = getTextureFileResource(pstexture.file);
					else if (!pstexture.name.empty())
						tex = getTextureNamedResource(pstexture.name);

					mat->addTexture(tex, getSampler(pstexture), pstexture.name, ShaderTypePixel);
				}

				//pixel shader uav
				for (auto& psuav : info.psuavs)
				{
					mat->addUAV(getUAV(psuav), psuav);
				}
			}
		}
	}

	if (!defaultMaterial)
	{
		auto it = materialMap.find("DefaultMaterial");
		if (it != materialMap.end())
			defaultMaterial = materialMap["DefaultMaterial"];
	}
}

void AaMaterialResources::addTextureResource(ID3D11ShaderResourceView* view, std::string name)
{
	loadedTextures[name] = view;

	for (auto& m : materialMap)
	{
		m.second->updateTexture(name, view);
	}
}

ID3D11UnorderedAccessView* AaMaterialResources::getUAV(std::string name)
{
	auto it = loadedUAVs.find(name);

	if (it != loadedUAVs.end())
	{
		return it->second;
	}

	return nullptr;
}

void AaMaterialResources::swapTextures(std::string name1, std::string name2)
{
	auto it = loadedTextures.find(name1);
	auto it2 = loadedTextures.find(name2);

	if (it != loadedTextures.end() && it2 != loadedTextures.end())
	{
		ID3D11ShaderResourceView* t1 = it->second;
		ID3D11ShaderResourceView* t2 = it2->second;

		loadedTextures[name1] = t2;
		loadedTextures[name2] = t1;
	}

	auto it3 = loadedUAVs.find(name1);
	auto it4 = loadedUAVs.find(name2);

	if (it3 != loadedUAVs.end() && it4 != loadedUAVs.end())
	{
		ID3D11UnorderedAccessView* t1 = it3->second;
		ID3D11UnorderedAccessView* t2 = it4->second;

		loadedUAVs[name1] = t2;
		loadedUAVs[name2] = t1;
	}
}

ID3D11ShaderResourceView* AaMaterialResources::getTextureFileResource(std::string file)
{
	auto it = loadedTextures.find(file);

	if (it == loadedTextures.end())
	{
		ID3D11ShaderResourceView* textureMap{};

		HRESULT d3dResult = LoadTextureFromFile(mRenderSystem->getDevice(), mRenderSystem->getContext(), TEXTURE_DIRECTORY + file, &textureMap);

		if (FAILED(d3dResult))
		{
			AaLogger::logError("Failed to load the texture image " + file);
			textureMap = defaultTexture;
		}
		else
		{
			loadedTextures[file] = textureMap;
			AaLogger::log("Loaded texture image " + file);
		}

		return textureMap;
	}

	return it->second;
}

ID3D11ShaderResourceView* AaMaterialResources::getTextureNamedResource(std::string name)
{
	auto it = loadedTextures.find(name);

	return it == loadedTextures.end() ? nullptr : it->second;
}
