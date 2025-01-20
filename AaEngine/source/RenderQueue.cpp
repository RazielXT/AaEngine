#include "RenderQueue.h"
#include "EntityInstancing.h"
#include "GraphicsResources.h"
#include <algorithm>

void RenderQueue::update(const EntityChangeDescritpion& changeInfo, GraphicsResources& resources)
{
	auto& [change, order, entity] = changeInfo;

	if (change == EntityChange::Add)
	{
		if (order != targetOrder)
			return;

		auto matInstance = entity->material;
		if (technique != MaterialTechnique::Default)
		{
			if (technique == MaterialTechnique::DepthShadowmap && entity->hasFlag(RenderObjectFlag::NoShadow))
				return;

			std::string techniqueMaterial;

			if (technique == MaterialTechnique::Depth || technique == MaterialTechnique::DepthShadowmap)
				techniqueMaterial = "Depth";
			else if (technique == MaterialTechnique::Voxelize)
				techniqueMaterial = "Voxelize";
			else if (technique == MaterialTechnique::EntityId)
				techniqueMaterial = "EntityId";

			if (auto techniqueOverride = matInstance->GetBase()->GetTechniqueOverride(technique))
			{
				if (techniqueOverride[0] == '\0') //skip
					return;

				techniqueMaterial = techniqueOverride;
			}
			else if (matInstance->HasInstancing())
			{
				techniqueMaterial += "Instancing";
			}

			matInstance = resources.materials.getMaterial(techniqueMaterial);
		}

		auto entry = EntityEntry{ entity, matInstance->Assign(entity->geometry.layout ? *entity->geometry.layout : std::vector<D3D12_INPUT_ELEMENT_DESC>{}, targets, technique) };

		auto it = std::lower_bound(entities.begin(), entities.end(), entry);
		entities.insert(it, entry);
	}
	if (change == EntityChange::Delete)
	{
		if (order != targetOrder)
			return;

		for (auto it = entities.begin(); it != entities.end(); it++)
		{
			if (it->entity == entity)
			{
				entities.erase(it);
				break;
			}
		}
	}
	if (change == EntityChange::DeleteAll)
	{
		reset();
	}
}

void RenderQueue::reset()
{
	entities.clear();
}

std::vector<UINT> RenderQueue::createEntityFilter() const
{
	std::vector<UINT> out;

	for (auto& entry : entities)
	{
		out.push_back(entry.entity->getId());
	}

	std::sort(out.begin(), out.end());

	return out;
}

static void RenderObject(ID3D12GraphicsCommandList* commandList, SceneEntity* e)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (e->geometry.type != EntityGeometry::Type::Manual)
	{
		commandList->IASetVertexBuffers(0, 1, &e->geometry.vertexBufferView);
		commandList->IASetIndexBuffer(&e->geometry.indexBufferView);
	}

	if (e->geometry.indexCount)
		commandList->DrawIndexedInstanced(e->geometry.indexCount, e->geometry.instanceCount, 0, 0, 0);
	else
		commandList->DrawInstanced(e->geometry.vertexCount, e->geometry.instanceCount, 0, 0);
}

void RenderQueue::renderObjects(ShaderConstantsProvider& constants, ID3D12GraphicsCommandList* commandList)
{
	EntityEntry lastEntry{};
	MaterialDataStorage storage;

	for (auto& entry : entities)
	{
		if (!entry.entity->isVisible(constants.info.visibility))
			continue;

		constants.entity = entry.entity;

		if (entry.base != lastEntry.base)
			entry.base->BindSignature(commandList);

		if (entry.material != lastEntry.material)
		{
			if (technique == MaterialTechnique::Voxelize)
			{
				entry.material->SetUAV(constants.voxelBufferUAV, 0);
			}

			entry.material->LoadMaterialConstants(storage);
			entry.material->UpdatePerFrame(storage, constants);
			entry.material->BindPipeline(commandList);
			entry.material->BindTextures(commandList);
		}

		if (technique == MaterialTechnique::Voxelize)
		{
			entry.entity->material->GetParameter(FastParam::Emission, storage.rootParams.data() + entry.material->GetParameterOffset(FastParam::Emission));
			entry.entity->material->GetParameter(FastParam::MaterialColor, storage.rootParams.data() + entry.material->GetParameterOffset(FastParam::MaterialColor));
			entry.entity->material->GetParameter(FastParam::TexIdDiffuse, storage.rootParams.data() + entry.material->GetParameterOffset(FastParam::TexIdDiffuse));
			memcpy(storage.rootParams.data() + entry.material->GetParameterOffset(FastParam::VoxelIdx), &constants.voxelIdx, sizeof(constants.voxelIdx));
		}
		if (technique == MaterialTechnique::EntityId)
		{
			if (entry.entity->geometry.type == EntityGeometry::Type::Instancing)
			{
				auto group = (InstanceGroup*)entry.entity->geometry.source;
				entry.material->SetParameter("ResIdIdsBuffer", storage.rootParams.data(), &group->gpuIdsBufferHeapIdx, 1);
			}
			else
			{
				UINT id = entry.entity->getGlobalId().value;
				entry.material->SetParameter("EntityId", storage.rootParams.data(), &id, sizeof(id) / sizeof(float));
			}
		}

		entry.material->UpdatePerObject(storage, constants);
		entry.material->BindConstants(commandList, storage, constants);

		RenderObject(commandList, entry.entity);

		if (technique == MaterialTechnique::Voxelize)
		{
			auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
			commandList->ResourceBarrier(1, &uavBarrier);
		}

		lastEntry = entry;
	}
}

RenderQueue::EntityEntry::EntityEntry(SceneEntity* e, AssignedMaterial* m)
{
	entity = e;
	material = m;
	base = material->GetBase();
}
