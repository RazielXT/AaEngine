#pragma once

#define USE_D3DCOMPILE

#include <map>
#include "AaRenderSystem.h"
#include "AaMaterial.h"
#include "AaMaterialFileParser.h"

class AaMaterialResources
{
public:

	AaMaterialResources(AaRenderSystem* mRenderSystem);
	~AaMaterialResources();

	static AaMaterialResources& get();

	AaMaterial* getMaterial(std::string name);
	void loadMaterials(std::string directory, bool subDirectories = false);

	ID3D11ShaderResourceView* getTextureFileResource(std::string file);
	ID3D11ShaderResourceView* getTextureNamedResource(std::string name);
	ID3D11UnorderedAccessView* getUAV(std::string name);
	ID3D11SamplerState* getSampler(const textureInfo& info);

	void addTextureResource(ID3D11ShaderResourceView* view, std::string name);
	void addUAV(ID3D11UnorderedAccessView* view, std::string name);

	void swapTextures(std::string name1, std::string name2);

private:

	AaRenderSystem* mRenderSystem{};
	std::map<std::string, ID3D11ShaderResourceView*> loadedTextures;
	std::map<std::string, ID3D11UnorderedAccessView*> loadedUAVs;

	std::vector<ID3D11SamplerState*> createdSamplers;
	ID3D11SamplerState* defaultMapSampler{};
	ID3D11ShaderResourceView* defaultTexture{};

	std::map<std::string, AaMaterial*> materialMap;
	AaMaterial* defaultMaterial{};
};
