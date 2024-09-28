#include "AaSceneManager.h"
#include "AaMaterialResources.h"
#include "AaMaterialConstants.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs)
{
	renderSystem = rs;
}

AaSceneManager::~AaSceneManager()
{
	for (auto& order : entityOrder)
	{
		for (auto& e : order.second)
		{
			delete e.entity;
		}
	}
}

AaEntity* AaSceneManager::createEntity(std::string name, std::string mesh, std::string material, Order order)
{
	auto ent = new AaEntity(name);
	ent->setModel(mesh);
	ent->setMaterial(material);

	auto entry = EntityEntry{ ent };
	auto& entities = entityOrder[order];

	auto it = std::lower_bound(entities.begin(), entities.end(), entry);
	entities.insert(it, entry);
	entityMap[name] = ent;

	return ent;
}

AaEntity* AaSceneManager::getEntity(std::string name) const
{
	auto it = entityMap.find(name);
	if (it != entityMap.end())
		return it->second;

	return nullptr;
}

void RenderObject(ID3D12GraphicsCommandList* commandList, AaEntity* e, AaCamera& camera)
{
	commandList->IASetVertexBuffers(0, 1, &e->model->vertexBufferView);
	commandList->IASetIndexBuffer(&e->model->indexBufferView);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawIndexedInstanced(e->model->indexCount, 1, 0, 0, 0);
}

void AaSceneManager::renderObjects(AaCamera& camera, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	EntityEntry lastEntry{};
	MaterialConstantBuffers constants;

	for (auto& [order, entities] : entityOrder)
	{
		for (auto& entry : entities)
		{
			if (!entry.entity->isVisible(info.visibility))
				continue;

			if (entry.base != lastEntry.base)
				entry.base->BindSignature(commandList, frameIndex);

			if (entry.material != lastEntry.material)
			{
				entry.material->LoadMaterialConstants(constants);
				entry.material->UpdatePerFrame(constants, params);
				entry.material->BindPipeline(commandList);
				entry.material->BindTextures(commandList, frameIndex);
			}

			entry.material->UpdatePerObject(constants, entry.entity->getWvpMatrix(info.wvpMatrix), entry.entity->getWorldMatrix(), params);
			entry.material->BindConstants(commandList, frameIndex, constants);

			RenderObject(commandList, entry.entity, camera);

			lastEntry = entry;
		}
	}
}

void AaSceneManager::renderObjectsDepth(AaCamera& camera, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	AaMaterial* lastEntry{};
	MaterialConstantBuffers constants;

	for (auto& [order, entities] : entityOrder)
	{
		for (auto& entry : entities)
		{
			if (!entry.entity->isVisible(info.visibility))
				continue;

			if (entry.entity->depthMaterial != lastEntry)
			{
				entry.entity->depthMaterial->GetBase()->BindSignature(commandList, frameIndex);
				entry.entity->depthMaterial->LoadMaterialConstants(constants);
				entry.entity->depthMaterial->UpdatePerFrame(constants, params);
				entry.entity->depthMaterial->BindPipeline(commandList);
				entry.entity->depthMaterial->BindTextures(commandList, frameIndex);
			}

			entry.entity->depthMaterial->UpdatePerObject(constants, entry.entity->getWvpMatrix(info.wvpMatrix), entry.entity->getWorldMatrix(), params);
			entry.entity->depthMaterial->BindConstants(commandList, frameIndex, constants);

			RenderObject(commandList, entry.entity, camera);

			lastEntry = entry.entity->depthMaterial;
		}
	}
}

void AaSceneManager::renderQuad(AaMaterial* material, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	MaterialConstantBuffers constants;

	material->GetBase()->BindSignature(commandList, frameIndex);

	material->LoadMaterialConstants(constants);
	material->UpdatePerFrame(constants, params);
	material->BindPipeline(commandList);
	material->BindTextures(commandList, frameIndex);
	material->BindConstants(commandList, frameIndex, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}

AaSceneManager::EntityEntry::EntityEntry(AaEntity* e)
{
	entity = e;
	material = e->material;
	base = material->GetBase();
}
