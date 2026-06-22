#include "FrameCompositor/Tasks/PrepareFrameTask.h"
#include "Scene/RenderWorld.h"
#include "Scene/Camera.h"
#include "RenderCore/ShadowMaps.h"

PrepareFrameTask::PrepareFrameTask(RenderProvider provider, RenderWorld& w, ShadowMaps& shadows) : shadowMaps(shadows), CompositorTask(provider, w)
{
}

PrepareFrameTask::~PrepareFrameTask()
{
}

void PrepareFrameTask::recordCommands(RenderContext& ctx, CommandsData& cmd, CompositorPass& pass)
{
	if (pass.info.entry == "PostCompute")
	{
		// View 0: main camera.
		renderWorld.grass.updateCulling(cmd.commandList, *ctx.camera, *ctx.camera, renderWorld.terrain, 0);
		// Views 1..: first shadow cascades. Frustum from the light cascade, distance fade from the main camera.
		for (auto v : { GeometryViewType_ShadowCascade0, GeometryViewType_ShadowCascade1 })
		{
			auto& cascade = shadowMaps.cascades[v];
			if (cascade.update)
				renderWorld.grass.updateCulling(cmd.commandList, cascade.camera, *ctx.camera, renderWorld.terrain, v);
		}
		renderWorld.vegetation.updateCulling(cmd.commandList, *ctx.camera, renderWorld.terrain);
		return;
	}

	auto& dlss = provider.renderSystem.upscale.dlss;
	if (dlss.enabled())
		ctx.camera->setPixelOffset(dlss.getJitter(), dlss.getRenderSize());
	auto& fsr = provider.renderSystem.upscale.fsr;
	if (fsr.enabled())
		ctx.camera->setPixelOffset(fsr.getJitter(), fsr.getRenderSize());

	renderWorld.terrain.update(cmd.commandList, *ctx.camera, provider.params.frameIndex);
	renderWorld.vegetation.update(cmd.commandList, ctx.camera->getPosition(), renderWorld.terrain);
	renderWorld.grass.update(cmd.commandList, *ctx.camera, renderWorld.terrain);

	prepareMotionVectors(ctx);
}

CompositorTask::Execution PrepareFrameTask::getExecution(CompositorPass& pass) const
{
	if (pass.info.entry == "PostCompute")
		return { RecordMode::Inline, Queue::Compute };

	return { RecordMode::Inline, Queue::Graphics };
}

void PrepareFrameTask::prepareMotionVectors(RenderContext& ctx)
{
	// Assuming view and viewPrevious are pointers to View instances
	XMMATRIX viewMatrixCurrent = ctx.camera->getViewMatrix();
	static XMMATRIX viewMatrixPrevious = viewMatrixCurrent;
	XMMATRIX projMatrixCurrent = ctx.camera->getProjectionMatrixNoOffset();
	static XMMATRIX projMatrixPrevious = projMatrixCurrent;

	// Calculate the view reprojection matrix
	XMMATRIX viewReprojectionMatrix = XMMatrixMultiply(XMMatrixInverse(nullptr, viewMatrixCurrent), viewMatrixPrevious);

	// Calculate the reprojection matrix for TAA
	XMMATRIX reprojectionMatrix = XMMatrixMultiply(XMMatrixMultiply(XMMatrixInverse(nullptr, projMatrixCurrent), viewReprojectionMatrix), projMatrixPrevious);

	viewMatrixPrevious = viewMatrixCurrent;
	projMatrixPrevious = projMatrixCurrent;

	XMFLOAT4X4 reprojectionData;
	XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&reprojectionData, XMMatrixTranspose(reprojectionMatrix));
	provider.resources.materials.getMaterial("MotionVectors")->SetParameter("ReprojectionMatrix", &reprojectionData._11, 16);
	provider.resources.materials.getMaterial("AccumulateReflections")->SetParameter("ReprojectionMatrix", &reprojectionData._11, 16);
}
