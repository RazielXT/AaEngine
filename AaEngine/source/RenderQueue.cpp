#include "RenderQueue.h"

void RenderQueue::update(const EntityChanges& changes)
{
	for (auto& [change, entity] : changes)
	{
		if (change == EntityChange::Add)
		{
			auto entry = EntityEntry{ entity, (materialOverride ? materialOverride : entity->material)->Assign(entity->model->vertexLayout, targets) };
			auto& entities = entityOrder[entity->order];

			auto it = std::lower_bound(entities.begin(), entities.end(), entry);
			entities.insert(it, entry);
		}
		if (change == EntityChange::DeleteAll)
		{
			entityOrder.clear();
		}
	}
}

static void RenderObject(ID3D12GraphicsCommandList* commandList, AaEntity* e, AaCamera& camera)
{
	commandList->IASetVertexBuffers(0, 1, &e->model->vertexBufferView);
	commandList->IASetIndexBuffer(&e->model->indexBufferView);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawIndexedInstanced(e->model->indexCount, 1, 0, 0, 0);
}

void RenderQueue::renderObjects(AaCamera& camera, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	if (entityOrder.empty())
		return;

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

// void AaSceneManager::renderObjectsDepth(AaCamera& camera, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
// {
// 	AaMaterial* lastEntry{};
// 	MaterialConstantBuffers constants;
// 
// 	for (auto& [order, entities] : entityOrder)
// 	{
// 		for (auto& entry : entities)
// 		{
// 			if (!entry.entity->isVisible(info.visibility))
// 				continue;
// 
// 			if (entry.entity->depthMaterial != lastEntry)
// 			{
// 				entry.entity->depthMaterial->GetBase()->BindSignature(commandList, frameIndex);
// 				entry.entity->depthMaterial->LoadMaterialConstants(constants);
// 				entry.entity->depthMaterial->UpdatePerFrame(constants, params);
// 				entry.entity->depthMaterial->BindPipeline(commandList);
// 				entry.entity->depthMaterial->BindTextures(commandList, frameIndex);
// 			}
// 
// 			entry.entity->depthMaterial->UpdatePerObject(constants, entry.entity->getWvpMatrix(info.wvpMatrix), entry.entity->getWorldMatrix(), params);
// 			entry.entity->depthMaterial->BindConstants(commandList, frameIndex, constants);
// 
// 			RenderObject(commandList, entry.entity, camera);
// 
// 			lastEntry = entry.entity->depthMaterial;
// 		}
// 	}
// }

RenderQueue::EntityEntry::EntityEntry(AaEntity* e, AaMaterial* m)
{
	entity = e;
	material = m;
	base = material->GetBase();
}
