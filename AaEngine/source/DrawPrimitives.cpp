#include "DrawPrimitives.h"
#include "ModelResources.h"
#include "MaterialResources.h"
#include "SceneEntity.h"

BoundingBoxDraw::BoundingBoxDraw(GraphicsResources& r, const std::vector<DXGI_FORMAT>& targets) : resources(r)
{
	model = resources.models.getLoadedModel("box.mesh", ResourceGroup::Core);
	material = resources.materials.getMaterial("BoundingBox")->Assign(model->vertexLayout, targets);
}

void BoundingBoxDraw::renderAxisAligned(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider& constants, SceneEntity* entity)
{
	{
		auto bbox = entity->getWorldBoundingBox();
		bbox.Extents.x = max(bbox.Extents.x, 1);
		bbox.Extents.y = max(bbox.Extents.y, 1);
		bbox.Extents.z = max(bbox.Extents.z, 1);

		Vector3 centerPosition = bbox.Center;

		constexpr float boundsOffsetAdd = 0.1f; //10%

		Vector3 scale = bbox.Extents * 2 * (1.f + boundsOffsetAdd);
		Vector3 boxPosition = centerPosition;
		boxPosition.y -= bbox.Extents.y * (1.f + boundsOffsetAdd); //box model has center at bottom

		{
			XMMATRIX translationM = XMMatrixTranslation(boxPosition.x, boxPosition.y, boxPosition.z);
			XMMATRIX scaleM = XMMatrixScaling(scale.x, scale.y, scale.z);

			XMFLOAT4X4 worldMatrix;
			XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(XMMatrixMultiply(scaleM, translationM)));
			material->SetParameter(ResourcesInfo::AutoParam::WORLD_MATRIX, &worldMatrix._11, sizeof(worldMatrix) / sizeof(float));

		}
		material->SetParameter(ResourcesInfo::AutoParam::WORLD_POSITION, &centerPosition.x, sizeof(centerPosition) / sizeof(float));
	}

	material->GetBase()->BindSignature(commandList);

	MaterialDataStorage storage;
	material->LoadMaterialConstants(storage);
	material->UpdatePerFrame(storage, constants);
	material->BindPipeline(commandList);
	material->BindTextures(commandList);
	//material->UpdatePerObject(storage, constants);
	material->BindConstants(commandList, storage, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);
	commandList->IASetIndexBuffer(&model->indexBufferView);
	commandList->DrawIndexedInstanced(model->indexCount, 1, 0, 0, 0);
}

void BoundingBoxDraw::renderObjectAligned(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider& constants, SceneEntity* entity)
{
	{
		auto bbox = entity->getBoundingBox();

		Vector3 scale = bbox.Extents * 2;

		constexpr float boundsOffsetAdd = 0.1f; //10%

		auto minScale = bbox.Extents.x;
		minScale = min(bbox.Extents.y, minScale);
		minScale = min(bbox.Extents.z, minScale);
		minScale = min(minScale, 1);
		scale += Vector3(minScale) * boundsOffsetAdd * 2;

		Vector3 boxPosition = entity->getPosition();

		Vector3 positionOffset{};
		positionOffset.y -= (bbox.Extents.y - bbox.Center.y) + minScale * boundsOffsetAdd; //box model has center at bottom

		{
			XMMATRIX translationM = XMMatrixTranslation(positionOffset.x, positionOffset.y, positionOffset.z);
			XMMATRIX scaleM = XMMatrixScaling(scale.x, scale.y, scale.z);

			XMFLOAT4X4 worldMatrix;
			XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(XMMatrixMultiply(XMMatrixMultiply(scaleM, translationM), entity->getWorldMatrix())));
			material->SetParameter(ResourcesInfo::AutoParam::WORLD_MATRIX, &worldMatrix._11, sizeof(worldMatrix) / sizeof(float));

		}

		material->SetParameter(ResourcesInfo::AutoParam::WORLD_POSITION, &boxPosition.x, sizeof(boxPosition) / sizeof(float));
	}

	material->GetBase()->BindSignature(commandList);

	MaterialDataStorage storage;
	material->LoadMaterialConstants(storage);
	material->UpdatePerFrame(storage, constants);
	material->BindPipeline(commandList);
	material->BindTextures(commandList);
	//material->UpdatePerObject(storage, constants);
	material->BindConstants(commandList, storage, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);
	commandList->IASetIndexBuffer(&model->indexBufferView);
	commandList->DrawIndexedInstanced(model->indexCount, 1, 0, 0, 0);
}
