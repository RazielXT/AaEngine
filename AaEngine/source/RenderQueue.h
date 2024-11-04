#pragma once

#include "AaEntity.h"
#include "AaCamera.h"
#include "AaRenderables.h"

enum class EntityChange
{
	Add,
	DeleteAll,
};

using EntityChanges = std::vector<std::pair<EntityChange, AaEntity*>>;

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

	std::map<Order, std::vector<EntityEntry>> entityOrder;
	std::vector<DXGI_FORMAT> targets;
	MaterialTechnique technique = MaterialTechnique::Default;
	Order targetOrder = Order::Normal;

	void update(const EntityChanges&);
	void renderObjects(AaCamera& cam, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex);
};
