#pragma once

#include <map>
#include "RenderSystem.h"
#include "AaMaterial.h"
#include "AaMaterialFileParser.h"
#include "AaTextureResources.h"

class AaMaterialResources
{
public:

	AaMaterialResources(RenderSystem* rs);
	~AaMaterialResources();

	static AaMaterialResources& get();

	MaterialInstance* getMaterial(std::string name);
	MaterialInstance* getMaterial(std::string name, ResourceUploadBatch& batch);

	void loadMaterials(std::string directory, bool subDirectories = false);

	void ReloadShaders();

private:

	RenderSystem* renderSystem{};

	std::vector<MaterialRef> knownMaterials;

	MaterialInstance* loadMaterial(std::string name, ResourceUploadBatch& batch);
	std::map<std::string, std::unique_ptr<MaterialInstance>> materialMap;
	std::map<std::string, std::unique_ptr<MaterialBase>> materialBaseMap;

	AaTextureResources textures;
};
