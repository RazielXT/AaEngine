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
			if (variant == MaterialVariant::Depth)
				matInstance = AaMaterialResources::get().getMaterial(entity->geometry.usesInstancing() ? "DepthInstanced" : "Depth");
			else if (variant == MaterialVariant::Voxel)
				matInstance = AaMaterialResources::get().getMaterial(entity->geometry.usesInstancing() ? "VoxelizeInstanced" : "Voxelize");

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

void updateEntityCbuffers(ShaderBuffersInfo& constants, AaEntity* entity)
{
	constants.cbuffers.instancing = entity->geometry.geometryCustomBuffer;
}

void RenderQueue::renderObjects(AaCamera& camera, const RenderInformation& info, const FrameGpuParameters& params, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	EntityEntry lastEntry{};
	ShaderBuffersInfo constants;

	for (auto& [order, entities] : entityOrder)
	{
		for (auto& entry : entities)
		{
			if (!entry.entity->isVisible(info.visibility))
				continue;

			//grass
			if (!entry.entity->geometry.indexCount && variant != MaterialVariant::Default)
				continue;

			if (entry.base != lastEntry.base)
				entry.base->BindSignature(commandList, frameIndex);

			if (entry.material != lastEntry.material)
			{
				entry.material->LoadMaterialConstants(constants);
				entry.material->UpdatePerFrame(constants, params, camera.getViewProjectionMatrix());
				entry.material->BindPipeline(commandList);
				entry.material->BindTextures(commandList, frameIndex);
			}

			if (variant == MaterialVariant::Voxel)
			{
				entry.entity->material->GetParameter(FastParam::Emission, constants.data.front().data() + entry.material->GetParameterOffset(FastParam::Emission));
				entry.entity->material->GetParameter(FastParam::MaterialColor, constants.data.front().data() + entry.material->GetParameterOffset(FastParam::MaterialColor));
			}

			updateEntityCbuffers(constants, entry.entity);
			entry.material->UpdatePerObject(constants, entry.entity->getWvpMatrix(info.wvpMatrix), entry.entity->getWorldMatrix(), params);
			entry.material->BindConstants(commandList, frameIndex, constants);

			RenderObject(commandList, entry.entity, camera);

			if (variant == MaterialVariant::Voxel)
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
