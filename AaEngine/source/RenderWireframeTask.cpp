#include "RenderWireframeTask.h"
#include "SceneManager.h"

RenderWireframeTask* instance = nullptr;

RenderWireframeTask::RenderWireframeTask(RenderProvider& p, SceneManager& s) : CompositorTask(p, s)
{
	instance = this;
}

RenderWireframeTask::~RenderWireframeTask()
{
	instance = nullptr;
}

AsyncTasksInfo RenderWireframeTask::initialize(CompositorPass& pass)
{
	wireframeMaterial = provider.resources.materials.getMaterial("DebugWireframeNoVC");

	return {};
}

static void RenderObject(ID3D12GraphicsCommandList* commandList, const VertexBufferModel& model)
{
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &model.vertexBufferView);

	if (model.indexBuffer)
		commandList->IASetIndexBuffer(&model.indexBufferView);

	if (model.indexBuffer)
		commandList->DrawIndexedInstanced(model.indexCount, 1, 0, 0, 0);
	else
		commandList->DrawInstanced(model.vertexCount, 1, 0, 0);
}

void RenderWireframeTask::run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass)
{
	if (!enabled)
		return;

	pass.target.textureSet->PrepareAsTarget(syncCommands.commandList, true, TransitionFlags::DepthContinue);

	auto opaque = sceneMgr.getQueue(MaterialTechnique::Default, Order::Normal);

	ShaderConstantsProvider constants(provider.params, {}, *ctx.camera, *pass.target.texture);
	MaterialDataStorage storage;
	AssignedMaterial* lastMaterial{};

	for (auto& entry : opaque->entities)
	{
		auto entity = entry.entity;

		if (entity->geometry.type != EntityGeometry::Model)
			continue;

		auto model = (VertexBufferModel*)entity->geometry.source;
		auto material = wireframeMaterial->Assign(model->vertexLayout, pass.target.textureSet->formats);

		if (material != lastMaterial)
		{
			lastMaterial = material;

			material->GetBase()->BindSignature(syncCommands.commandList);
			material->LoadMaterialConstants(storage);
			material->BindPipeline(syncCommands.commandList);
			material->BindTextures(syncCommands.commandList);
			material->UpdatePerFrame(storage, constants);
		}

		constants.entity = entity;
		material->UpdatePerObject(storage, constants);

		material->BindConstants(syncCommands.commandList, storage, constants);
		RenderObject(syncCommands.commandList, *model);
	}
}

RenderWireframeTask& RenderWireframeTask::Get()
{
	return *instance;
}
