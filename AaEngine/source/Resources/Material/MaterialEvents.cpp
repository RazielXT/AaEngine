#include "Resources/Material/MaterialEvents.h"

MaterialEvents& MaterialEvents::Get()
{
	static MaterialEvents instance;
	return instance;
}

void MaterialEvents::addReloadListener(MaterialsReloadedCallback callback)
{
	reloadListeners.push_back(std::move(callback));
}

void MaterialEvents::addEntityParamChangeListener(EntityMaterialChangedCallback callback)
{
	entityParamChangeListeners.push_back(std::move(callback));
}

void MaterialEvents::notifyReloaded(const std::vector<MaterialBase*>& reloaded)
{
	for (auto& listener : reloadListeners)
		listener(reloaded);
}

void MaterialEvents::notifyEntityParamChanged(RenderEntity& entity)
{
	for (auto& listener : entityParamChangeListeners)
		listener(entity);
}
