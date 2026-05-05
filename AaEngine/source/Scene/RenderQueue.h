#pragma once

#include "Scene/RenderEntity.h"
#include "Scene/Camera.h"
#include "Scene/RenderObject.h"
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
	RenderEntity* entity{};
	ObjectId id;
	int suborder;
};

using EntityChanges = std::vector<EntityChangeDescritpion>;

struct RenderQueue
{
	struct EntityEntry
	{
		const MaterialBase* base{};
		AssignedMaterial* material{};
		std::unique_ptr<MaterialPropertiesOverride> materialOverride{};
		RenderEntity* entity{};
		int suborder;

		EntityEntry(RenderEntity*, AssignedMaterial*, MaterialTechnique, int suborder);

		void rebuildMaterial(MaterialTechnique technique);

		bool operator<(const EntityEntry& other) const
		{
			if (suborder != other.suborder) return suborder < other.suborder;
			if (base != other.base) return base < other.base;
			return material < other.material || (material == other.material && materialOverride < other.materialOverride);
		}
	};

	std::vector<DXGI_FORMAT> targetFormats;
	MaterialTechnique technique = MaterialTechnique::Default;
	Order targetOrder = Order::Normal;
	std::vector<EntityEntry> entities;

	void update(const EntityChangeDescritpion&, GraphicsResources& resources);
	void rebuildEntries(const std::vector<MaterialBase*>& reloaded);
	void rebuildEntries(const RenderEntity* reloaded);
	void reset();

	void renderObjects(ShaderConstantsProvider& info, ID3D12GraphicsCommandList* commandList);

	void iterateMaterials(std::function<void(AssignedMaterial*)>);
};
