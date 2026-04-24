#pragma once

#include <map>
#include <functional>
#include "RenderCore/RenderSystem.h"
#include "Resources/Material/Material.h"
#include "Resources/Material/MaterialFileParser.h"

struct GraphicsResources;

using MaterialsReloadedCallback = std::function<void(const std::vector<MaterialBase*>& reloaded)>;

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

	void addReloadListener(MaterialsReloadedCallback callback);

private:

	void reloadShaders(const std::vector<const LoadedShader*>& shadersChanged);

	RenderSystem& renderSystem;
	GraphicsResources& resources;

	std::vector<MaterialRef> knownMaterials;

	MaterialInstance* loadMaterial(std::string name, ResourceUploadBatch& batch);
	std::map<std::string, std::unique_ptr<MaterialInstance>> materialMap;
	std::map<std::string, std::unique_ptr<MaterialBase>> materialBaseMap;

	void notifyReloaded(const std::vector<MaterialBase*>& reloaded);
	std::vector<MaterialsReloadedCallback> reloadListeners;
};
