#pragma once

#include <map>
#include "AaRenderSystem.h"
#include "AaMaterial.h"
#include "AaMaterialFileParser.h"
#include "AaTextureResources.h"

class AaMaterialResources
{
public:

	AaMaterialResources(AaRenderSystem* mRenderSystem);
	~AaMaterialResources();

	static AaMaterialResources& get();

	MaterialInstance* getMaterial(std::string name);
	void loadMaterials(std::string directory, bool subDirectories = false);

	void PrepareShaderResourceView(RenderTargetTexture&);
	void PrepareDepthShaderResourceView(RenderDepthTargetTexture&);

	void ReloadShaders();

private:

	AaRenderSystem* mRenderSystem{};

	std::vector<MaterialRef> knownMaterials;

	std::map<std::string, std::unique_ptr<MaterialInstance>> materialMap;
	std::map<std::string, std::unique_ptr<MaterialBase>> materialBaseMap;

	AaTextureResources textures;

	ResourcesManager resourcesMgr;
};
