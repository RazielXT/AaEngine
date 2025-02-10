#include "PhysicsRenderer.h"
#include <DirectXMathConvert.inl>

using namespace JPH;

class ModelBatch : public RefTargetVirtual, public RefTarget<ModelBatch>
{
public:

	ModelBatch()
	{
		initializeLayout();
	}

	VertexBufferModel model;

	virtual void AddRef() override { RefTarget::AddRef(); }
	virtual void Release() override { if (--mRefCount == 0) delete this; }

	void initializeLayout()
	{
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
		model.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
		model.addLayoutElement(DXGI_FORMAT_R8G8B8A8_UNORM, VertexElementSemantic::COLOR);
	}
};

PhysicsRenderer::PhysicsRenderer(RenderSystem& rs, GraphicsResources& r) : renderSystem(rs), resources(r)
{
	Initialize();
}

PhysicsRenderer::~PhysicsRenderer()
{
}

bool PhysicsRenderer::PrepareForRendering(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider* constants, const std::vector<DXGI_FORMAT>& targets)
{
	if (batch)
	{
		if (!batchSubmitted)
		{
			batchUploadFuture = batch->End(renderSystem.core.copyQueue);
			batchSubmitted = true;
		}

		if (batchUploadFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return false;

		batch.reset();
		batchSubmitted = false;
	}

	if (!renderCtx.solidColorMaterial)
	{
		renderCtx.solidColorMaterial = resources.materials.getMaterial("DebugColor");
		renderCtx.wireframeMaterial = resources.materials.getMaterial("DebugWireframe");
	}

	renderCtx.lastMaterial = nullptr;
	renderCtx.commandList = commandList;
	renderCtx.targets = targets;
	renderCtx.constants = constants;

	return true;
}

JPH::DebugRenderer::Batch PhysicsRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
	PrepareUploadBatch();

	ModelBatch* primitive = new ModelBatch();
	primitive->model.CreateVertexBuffer(renderSystem.core.device, batch.get(), inTriangles, 3 * inTriangleCount, sizeof(Vertex));
	primitive->model.calculateBounds();

	return primitive;
}

JPH::DebugRenderer::Batch PhysicsRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32* inIndices, int inIndexCount)
{
	PrepareUploadBatch();

	ModelBatch* primitive = new ModelBatch();
	primitive->model.CreateVertexBuffer(renderSystem.core.device, batch.get(), inVertices, inVertexCount, sizeof(Vertex));
	primitive->model.CreateIndexBuffer(renderSystem.core.device, batch.get(), inIndices, (size_t)inIndexCount);
	primitive->model.calculateBounds();

	return primitive;
}

void PhysicsRenderer::DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow /*= ECastShadow::Off*/)
{
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

void PhysicsRenderer::DrawGeometry(RMat44Arg inModelMatrix, const AABox& inWorldSpaceBounds, float inLODScaleSq, ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode /*= ECullMode::CullBackFace*/, ECastShadow inCastShadow /*= ECastShadow::On*/, EDrawMode inDrawMode /*= EDrawMode::Solid*/)
{
	auto& model = ((ModelBatch*)inGeometry->mLODs.front().mTriangleBatch.GetPtr())->model;

	auto material = GetMaterial(model, inDrawMode);

	if (material != renderCtx.lastMaterial)
	{
		renderCtx.lastMaterial = material;

		material->GetBase()->BindSignature(renderCtx.commandList);
		material->LoadMaterialConstants(renderCtx.storage);
		material->BindPipeline(renderCtx.commandList);
		material->BindTextures(renderCtx.commandList);
		material->UpdatePerFrame(renderCtx.storage, *renderCtx.constants);
	}

	auto matColor = inModelColor.ToVec4();
	material->SetParameter(FastParam::MaterialColor, &matColor, renderCtx.storage);
	auto matrixW = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4*) &inModelMatrix));
	material->SetParameter(ResourcesInfo::AutoParam::WORLD_MATRIX, &matrixW, 16, renderCtx.storage);
	material->SetParameter(ResourcesInfo::AutoParam::PREV_WORLD_MATRIX, &matrixW, 16, renderCtx.storage);

	material->BindConstants(renderCtx.commandList, renderCtx.storage, *renderCtx.constants);

	RenderObject(renderCtx.commandList, model);
}

void PhysicsRenderer::DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor)
{
}

void PhysicsRenderer::DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor /*= Color::sWhite*/, float inHeight /*= 0.5f*/)
{
}

void PhysicsRenderer::PrepareUploadBatch()
{
	if (!batch)
	{
		batch = std::make_unique<ResourceUploadBatch>(renderSystem.core.device);
		batch->Begin(D3D12_COMMAND_LIST_TYPE_COPY);
	}
}

AssignedMaterial* PhysicsRenderer::GetMaterial(const VertexBufferModel& model, EDrawMode drawMode)
{
	if (drawMode == DebugRenderer::EDrawMode::Wireframe)
		return renderCtx.wireframeMaterial->Assign(model.vertexLayout, renderCtx.targets);

	return renderCtx.solidColorMaterial->Assign(model.vertexLayout, renderCtx.targets);
}
