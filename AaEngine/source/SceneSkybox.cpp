#include "SceneSkybox.h"
#include "ModelResources.h"
#include "MaterialResources.h"

SceneSkybox::SceneSkybox(GraphicsResources& r) : resources(r)
{
	//model = resources.models.getLoadedModel("box.mesh", ResourceGroup::Core);
}

void SceneSkybox::setMaterial(const std::string& name, const std::vector<DXGI_FORMAT>& targets)
{
	//material = resources.materials.getMaterial(name)->Assign(model->vertexLayout, targets);
	material = resources.materials.getMaterial(name)->Assign({}, targets);
}

void SceneSkybox::render(ID3D12GraphicsCommandList* commandList, const ShaderConstantsProvider& constants)
{
	material->GetBase()->BindSignature(commandList);

	MaterialDataStorage storage;
	material->LoadMaterialConstants(storage);
	material->UpdatePerFrame(storage, constants);
	material->BindPipeline(commandList);
	material->BindTextures(commandList);
	//material->UpdatePerObject(storage, constants);
	material->BindConstants(commandList, storage, constants);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);
	//commandList->IASetIndexBuffer(&model->indexBufferView);
	//commandList->DrawIndexedInstanced(model->indexCount, 1, 0, 0, 0);
	commandList->DrawInstanced(3, 1, 0, 0);
}
