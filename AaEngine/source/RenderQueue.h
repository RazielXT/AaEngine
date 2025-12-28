#pragma once

#include "SceneEntity.h"
#include "Camera.h"
#include "RenderObject.h"
#include <functional>

enum class EntityChange
{
	Add,
	Delete,
	DeleteAll,
};

struct EntityChangeDescritpion
{
	EntityChange type;
	Order order;
	SceneEntity* entity{};
};

using EntityChanges = std::vector<EntityChangeDescritpion>;

struct RenderQueue
{
	struct EntityEntry
	{
		const MaterialBase* base{};
		AssignedMaterial* material{};
		std::unique_ptr<MaterialPropertiesOverride> materialOverride{};
		SceneEntity* entity{};

		EntityEntry(SceneEntity*, AssignedMaterial*, MaterialTechnique);

		bool operator<(const EntityEntry& other) const
		{
			if (base != other.base) return base < other.base;
			return material < other.material || (material == other.material && materialOverride < other.materialOverride);
		}
	};

	std::vector<DXGI_FORMAT> targets;
	MaterialTechnique technique = MaterialTechnique::Default;
	Order targetOrder = Order::Normal;
	std::vector<EntityEntry> entities;

	void update(const EntityChangeDescritpion&, GraphicsResources& resources);
	void reset();

	void renderObjects(ShaderConstantsProvider& info, ID3D12GraphicsCommandList* commandList);

	void iterateMaterials(std::function<void(AssignedMaterial*)>);
};
