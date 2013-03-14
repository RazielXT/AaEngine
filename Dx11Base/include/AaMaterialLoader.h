#ifndef _AA_MATERIAL_LOADER_
#define _AA_MATERIAL_LOADER_


#define USE_D3DCOMPILE


#include <map>
#include "AaRenderSystem.h"
#include "AaMaterial.h"
#include "AaMaterialFileParser.h"

enum Filtering
{
	None=0,
	Bilinear=1,
	Trilinear=2,
	Anisotropic=3
};


class AaMaterialLoader
{

public:

	AaMaterialLoader(AaRenderSystem* mRenderSystem);
	~AaMaterialLoader();
	std::map<std::string,AaMaterial*> loadMaterials(std::string directory, bool subDirectories=false);
	void loadShaderReferences(std::string directory, bool subDirectories=false);
	void compileShaderReferences();
	ID3D11ShaderResourceView* getTextureResource(std::string file);
	void setDefaultFiltering(Filtering type, int max_anisotropy=1) { defaultFiltering=type; }

	bool compileVertexShaderRef(shaderRef* reference);
	bool compilePixelShaderRef(shaderRef* reference);

private:

	AaRenderSystem* mRenderSystem;
	AaMaterialFileParser fileParser;
	std::map<std::string,ID3D11ShaderResourceView*> loadedTextures;
	std::map<std::string,shaderRef> loadedVertexShaders;
	std::map<std::string,shaderRef> loadedPixelShaders;
	ID3D11ShaderResourceView* defaultTexture;
	Filtering defaultFiltering;
	
};

#endif