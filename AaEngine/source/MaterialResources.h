#pragma once

#include <map>
#include "RenderSystem.h"
#include "Material.h"
#include "MaterialFileParser.h"

struct GraphicsResources;

class MaterialResources
{
public:

	MaterialResources(RenderSystem& rs, GraphicsResources& resources);
	~MaterialResources();

	MaterialInstance* getMaterial(std::string name);
	MaterialInstance* getMaterial(std::string name, ResourceUploadBatch& batch);

	void loadMaterials(std::string directory, bool subDirectories = false);

	void reloadChangedShaders();

	void reloadShadersWithDefine(const std::string&);
	std::set<std::string> getKnownDefines() const;

private:

	RenderSystem& renderSystem;
	GraphicsResources& resources;

	std::vector<MaterialRef> knownMaterials;

	MaterialInstance* loadMaterial(std::string name, ResourceUploadBatch& batch);
	std::map<std::string, std::unique_ptr<MaterialInstance>> materialMap;
	std::map<std::string, std::unique_ptr<MaterialBase>> materialBaseMap;
};
