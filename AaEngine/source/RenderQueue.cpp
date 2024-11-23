#include "RenderQueue.h"
#include "EntityInstancing.h"
#include "AaMaterialResources.h"
#include <algorithm>

void RenderQueue::update(const EntityChanges& changes)
{
	for (auto& [change, order, entity] : changes)
	{
		if (change == EntityChange::Add)
		{
			if (order != targetOrder)
				continue;

			auto matInstance = entity->material;
			if (technique != MaterialTechnique::Default)
			{
				std::string techniqueMaterial;

				if (technique == MaterialTechnique::Depth)
					techniqueMaterial = "Depth";
				else if (technique == MaterialTechnique::Voxelize)
					techniqueMaterial = "Voxelize";

				if (auto techniqueOverride = matInstance->GetBase()->GetTechniqueOverride(technique))
				{
					if (techniqueOverride[0] == '\0') //skip
						continue;

					techniqueMaterial = techniqueOverride;
				}
				else if (matInstance->HasInstancing())
				{
					techniqueMaterial += "Instancing";
				}

				matInstance = AaMaterialResources::get().getMaterial(techniqueMaterial);
			}

			auto entry = EntityEntry{ entity, matInstance->Assign(entity->geometry.layout ? *entity->geometry.layout : std::vector<D3D12_INPUT_ELEMENT_DESC>{}, targets) };

			auto it = std::lower_bound(entities.begin(), entities.end(), entry);
			entities.insert(it, entry);
		}
		if (change == EntityChange::DeleteAll)
		{
			entities.clear();
		}
	}
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

static void RenderObject(ID3D12GraphicsCommandList* commandList, AaEntity* e)
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

RenderQueue::EntityEntry::EntityEntry(AaEntity* e, AaMaterial* m)
{
	entity = e;
	material = m;
	base = material->GetBase();
}
