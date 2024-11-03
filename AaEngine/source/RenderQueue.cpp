#include "RenderQueue.h"
#include "EntityInstancing.h"
#include "AaMaterialResources.h"

void RenderQueue::update(const EntityChanges& changes)
{
	for (auto& [change, entity] : changes)
	{
		if (change == EntityChange::Add)
		{
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

void RenderQueue::renderObjects(AaCamera& camera, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	EntityEntry lastEntry{};
	ShaderConstantsProvider constants(info, camera);

	for (auto& [order, entities] : entityOrder)
	{
		for (auto& entry : entities)
		{
			if (!entry.entity->isVisible(info.visibility))
				continue;

			constants.entity = entry.entity;

			if (entry.base != lastEntry.base)
				entry.base->BindSignature(commandList, frameIndex);

			if (entry.material != lastEntry.material)
			{
				entry.material->LoadMaterialConstants(constants);
				entry.material->UpdatePerFrame(constants, params);
				entry.material->BindPipeline(commandList);
				entry.material->BindTextures(commandList, frameIndex);
			}

			if (technique == MaterialTechnique::Voxelize)
			{
				entry.entity->material->GetParameter(FastParam::Emission, constants.data.front().data() + entry.material->GetParameterOffset(FastParam::Emission));
				entry.entity->material->GetParameter(FastParam::MaterialColor, constants.data.front().data() + entry.material->GetParameterOffset(FastParam::MaterialColor));
				entry.entity->material->GetParameter(FastParam::TexIdDiffuse, constants.data.front().data() + entry.material->GetParameterOffset(FastParam::TexIdDiffuse));
			}

			entry.material->UpdatePerObject(constants, params);
			entry.material->BindConstants(commandList, frameIndex, constants);

			RenderObject(commandList, entry.entity, camera);

			if (technique == MaterialTechnique::Voxelize)
			{
				auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
				commandList->ResourceBarrier(1, &uavBarrier);
			}

			lastEntry = entry;
		}
	}
}

RenderQueue::EntityEntry::EntityEntry(AaEntity* e, AaMaterial* m)
{
	entity = e;
	material = m;
	base = material->GetBase();
}
