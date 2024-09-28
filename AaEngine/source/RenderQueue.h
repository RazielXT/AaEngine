#pragma once

#include "AaEntity.h"
#include "AaCamera.h"

struct RenderInformation
{
	RenderableVisibility visibility;
	std::vector<XMFLOAT4X4> wvpMatrix;
};

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
	MaterialInstance* materialOverride{};
	std::vector<DXGI_FORMAT> targets;

	void update(const EntityChanges&);
	void renderObjects(AaCamera& cam, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex);
};
