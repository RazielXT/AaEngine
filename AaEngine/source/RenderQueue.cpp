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

			if (auto techniqueOverride = matInstance->GetTechniqueOverride(technique))
			{
				if (techniqueOverride[0] == '\0') //skip
					return;

				matInstance = resources.materials.getMaterial(techniqueOverride);

				if (!matInstance)
					__debugbreak();
			}
		}

		auto entry = EntityEntry{ entity, matInstance->Assign(entity->geometry.layout ? *entity->geometry.layout : std::vector<D3D12_INPUT_ELEMENT_DESC>{}, targets, technique) };

		auto it = std::lower_bound(entities.begin(), entities.end(), entry);
		entities.insert(it, std::move(entry));
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

static void RenderObject(ID3D12GraphicsCommandList* commandList, EntityGeometry& geometry)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY(geometry.topology));

	if (geometry.type != EntityGeometry::Type::Manual)
	{
		commandList->IASetVertexBuffers(0, 1, &geometry.vertexBufferView);
	}

	if (geometry.indexCount)
	{
		commandList->IASetIndexBuffer(&geometry.indexBufferView);
		commandList->DrawIndexedInstanced(geometry.indexCount, geometry.instanceCount, 0, 0, 0);
	}
	else
		commandList->DrawInstanced(geometry.vertexCount, geometry.instanceCount, 0, 0);
}

void RenderQueue::renderObjects(ShaderConstantsProvider& constants, ID3D12GraphicsCommandList* commandList)
{
	const MaterialBase* lastMaterialBase{};
	AssignedMaterial* lastMaterial{};

	MaterialDataStorage storage;

	for (auto& entry : entities)
	{
		if (!entry.entity->isVisible(constants.info.visibility))
			continue;

		constants.entity = entry.entity;

		if (entry.base != lastMaterialBase)
			entry.base->BindSignature(commandList);

		if (entry.material != lastMaterial)
		{
			entry.material->BindPipeline(commandList);

			if (!lastMaterial || entry.material->origin != lastMaterial->origin)
			{
				entry.material->LoadMaterialConstants(storage);
				entry.material->UpdatePerFrame(storage, constants);
				entry.material->BindTextures(commandList);
			}
		}

		if (entry.materialOverride)
			entry.material->ApplyParametersOverride(*entry.materialOverride, storage);

		if (technique == MaterialTechnique::Voxelize)
		{
			entry.material->CopyParameter(FastParam::Emission, *entry.entity->material, storage, 0.0f);
			entry.material->CopyParameter(FastParam::MaterialColor, *entry.entity->material, storage, 1.0f);
			entry.material->CopyParameter(FastParam::TexIdDiffuse, *entry.entity->material, storage, 0.0f);
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
				entry.material->SetParameter("EntityId", storage.rootParams.data(), &id, 1);
			}
		}

		entry.material->UpdatePerObject(storage, constants);
		entry.material->BindConstants(commandList, storage, constants);

		RenderObject(commandList, entry.entity->geometry);

		if (technique == MaterialTechnique::Voxelize)
		{
			auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
			commandList->ResourceBarrier(1, &uavBarrier);
		}

		lastMaterialBase = entry.base;
		lastMaterial = entry.material;
	}
}

void RenderQueue::iterateMaterials(std::function<void(AssignedMaterial*)> func)
{
	AssignedMaterial* m{};

	for (auto& entry : entities)
	{
		if (entry.material != m)
		{
			m = entry.material;
			func(m);
		}
	}
}

RenderQueue::EntityEntry::EntityEntry(SceneEntity* e, AssignedMaterial* m)
{
	entity = e;
	material = m;
	base = material->GetBase();

	if (e->materialOverride)
		materialOverride = material->CreateParameterOverride(*e->materialOverride);
}
