#pragma once

#include "AaEntity.h"
#include "AaCamera.h"
#include "SceneLights.h"

struct RenderInformation
{
	RenderableVisibility visibility;
	std::vector<XMFLOAT4X4> wvpMatrix;
};

class AaSceneManager
{
public:

	AaSceneManager(AaRenderSystem* rs);
	~AaSceneManager();

	enum Order
	{
		Normal = 50,
	};
	AaEntity* createEntity(std::string name, std::string mesh, std::string material, Order order = Order::Normal);
	AaEntity* getEntity(std::string name) const;

	aa::SceneLights lights;

	void renderObjects(AaCamera& cam, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void renderObjectsDepth(AaCamera& cam, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex);

	void renderQuad(AaMaterial*, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex);

private:

	struct EntityEntry
	{
		const MaterialBase* base{};
		AaMaterial* material{};
		AaEntity* entity{};

		EntityEntry(AaEntity*);
		EntityEntry() = default;

		bool operator<(const EntityEntry& other) const
		{
			if (base != other.base) return base < other.base;
			return material < other.material;
		}
	};
	std::map<Order, std::vector<EntityEntry>> entityOrder;
	std::map<std::string, AaEntity*> entityMap;

	AaRenderSystem* renderSystem;

};