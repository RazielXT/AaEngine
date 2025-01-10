#pragma once

#include "JoltHeader.h"
#include <Jolt/Renderer/DebugRenderer.h>
#include "MathUtils.h"
#include "RenderSystem.h"
#include "ResourceUploadBatch.h"
#include "GraphicsResources.h"
#include <memory>

class PhysicsRenderer final : public JPH::DebugRenderer
{
public:

	PhysicsRenderer(RenderSystem&, GraphicsResources&);
	~PhysicsRenderer();

	void PrepareForRendering(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider* constants, const std::vector<DXGI_FORMAT>& targets);

	void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;

	void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;

	Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;

	Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;

	void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override;

	void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor = JPH::Color::sWhite, float inHeight = 0.5f) override;

private:

	const Batch EmptyBatch;

	RenderSystem& renderSystem;
	GraphicsResources& resources;

	std::unique_ptr<ResourceUploadBatch> batch;
	void PrepareUploadBatch();

	struct 
	{
		ID3D12GraphicsCommandList* commandList;
		ShaderConstantsProvider* constants;
		MaterialDataStorage storage;
		std::vector<DXGI_FORMAT> targets;

		AssignedMaterial* lastMaterial{};
		MaterialInstance* solidColorMaterial{};
		MaterialInstance* wireframeMaterial{};	
	}
	renderCtx;

	AssignedMaterial* GetMaterial(const VertexBufferModel&, EDrawMode);
};