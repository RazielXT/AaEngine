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

		auto entry = EntityEntry(entity, matInstance->Assign(entity->geometry.layout ? *entity->geometry.layout : std::vector<D3D12_INPUT_ELEMENT_DESC>{}, targets, technique), technique);

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

static void RenderObject(ID3D12GraphicsCommandList* commandList, EntityGeometry& geometry, UINT frameIndex)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY(geometry.topology));

	if (geometry.type == EntityGeometry::Type::Indirect)
	{
		if (geometry.indexCount)
		{
			commandList->IASetIndexBuffer(&geometry.indexBufferView);
		}

		((IndirectEntityGeometry*)geometry.source)->draw(commandList, frameIndex);
	}
	else
	{
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

		entry.material->UpdatePerObject(storage, constants);
		entry.material->BindConstants(commandList, storage, constants);

		RenderObject(commandList, entry.entity->geometry, constants.params.frameIndex);

		if (constants.uavBarrier)
		{
			auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(constants.uavBarrier);
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

RenderQueue::EntityEntry::EntityEntry(SceneEntity* e, AssignedMaterial* m, MaterialTechnique technique)
{
	entity = e;
	material = m;
	base = material->GetBase();

	if (e->materialOverride)
		materialOverride = material->CreateParameterOverride(*e->materialOverride);

	if (technique == MaterialTechnique::EntityId)
	{
		if (!materialOverride)
			materialOverride = std::make_unique<MaterialPropertiesOverride>();

		if (entity->geometry.type == EntityGeometry::Type::Instancing)
		{
			auto group = (InstanceGroup*)entity->geometry.source;
			material->AppendParameterOverride(*materialOverride, "ResIdIdsBuffer", &group->gpuIdsBufferHeapIdx, sizeof(UINT));
		}
		else
		{
			UINT id = entity->getGlobalId().value;
			material->AppendParameterOverride(*materialOverride, "EntityId", &id, sizeof(UINT));
		}
	}

	if (technique == MaterialTechnique::Voxelize)
	{
		if (!materialOverride)
			materialOverride = std::make_unique<MaterialPropertiesOverride>();

		material->AppendParameterOverride(*materialOverride, "Emission", *entity->material, 0.0f);
		material->AppendParameterOverride(*materialOverride, "MaterialColor", *entity->material, 1.0f);
		material->AppendParameterOverride(*materialOverride, "TexIdDiffuse", *entity->material, 0.0f);
	}
}
