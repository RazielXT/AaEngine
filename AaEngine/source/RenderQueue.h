#pragma once

#include "AaEntity.h"
#include "AaCamera.h"
#include "AaRenderables.h"

enum class EntityChange
{
	Add,
	DeleteAll,
};

struct EntityChangeDescritpion
{
	EntityChange type;
	Order order;
	AaEntity* entity{};
};

using EntityChanges = std::vector<EntityChangeDescritpion>;

struct RenderQueue
{
	struct EntityEntry
	{
		const MaterialBase* base{};
		AaMaterial* material{};
		AaEntity* entity{};

		EntityEntry() = default;
		EntityEntry(AaEntity*, AaMaterial*);

		bool operator<(const EntityEntry& other) const
		{
			if (base != other.base) return base < other.base;
			return material < other.material;
		}
	};

	std::vector<DXGI_FORMAT> targets;
	MaterialTechnique technique = MaterialTechnique::Default;
	Order targetOrder = Order::Normal;
	std::vector<EntityEntry> entities;

	void update(const EntityChanges&);
	std::vector<UINT> createEntityFilter() const;
	void renderObjects(ShaderConstantsProvider& info, const FrameParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex);
};
