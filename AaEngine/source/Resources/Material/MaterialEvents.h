#pragma once

#include <functional>
#include <vector>

class MaterialBase;
class SceneEntity;

using MaterialsReloadedCallback = std::function<void(const std::vector<MaterialBase*>& reloaded)>;
using EntityMaterialChangedCallback = std::function<void(SceneEntity& entity)>;

class MaterialEvents
{
public:

	static MaterialEvents& Get();

	void addReloadListener(MaterialsReloadedCallback callback);
	void addEntityParamChangeListener(EntityMaterialChangedCallback callback);

	void notifyReloaded(const std::vector<MaterialBase*>& reloaded);
	void notifyEntityParamChanged(SceneEntity& entity);

private:

	std::vector<MaterialsReloadedCallback> reloadListeners;
	std::vector<EntityMaterialChangedCallback> entityParamChangeListeners;
};
