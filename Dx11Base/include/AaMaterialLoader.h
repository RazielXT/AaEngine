#ifndef _AA_MATERIAL_LOADER_
#define _AA_MATERIAL_LOADER_


#define USE_D3DCOMPILE


#include <map>
#include "AaRenderSystem.h"
#include "AaMaterial.h"
#include "AaMaterialFileParser.h"



class AaMaterialLoader
{

public:

	AaMaterialLoader(AaRenderSystem* mRenderSystem);
	~AaMaterialLoader();
	std::map<std::string,AaMaterial*> loadMaterials(std::string directory, bool subDirectories=false);
	void loadShaderReferences(std::string directory, bool subDirectories=false);
	void compileShaderReferences();
	ID3D11ShaderResourceView* getTextureResource(std::string file);
	ID3D11SamplerState* createSampler(textureInfo info);

	void setDefaultFiltering(Filtering type, int max_anisotropy=1) { defaultFiltering=type; }

	void addTextureResource(ID3D11ShaderResourceView* view, std::string name);

	void addUAV(ID3D11UnorderedAccessView* view, std::string name);
	void addSRV(ID3D11ShaderResourceView* view, std::string name);

	bool compileVertexShaderRef(shaderRef* reference);
	bool compilePixelShaderRef(shaderRef* reference);

private:

	AaRenderSystem* mRenderSystem;
	AaMaterialFileParser fileParser;
	std::map<std::string,ID3D11ShaderResourceView*> loadedTextures;
	std::map<std::string,ID3D11UnorderedAccessView*> loadedUAVs;
	std::map<std::string,ID3D11ShaderResourceView*> loadedSRVs;
	std::map<std::string,shaderRef> loadedVertexShaders;
	std::map<std::string,shaderRef> loadedPixelShaders;

	std::vector<ID3D11SamplerState*> createdSamplers;
	ID3D11SamplerState* defaultMapSampler;
	ID3D11ShaderResourceView* defaultTexture;
	Filtering defaultFiltering;
	
};

#endif