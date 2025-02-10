#pragma once

#include "GraphicsResources.h"
#include <string>
#include "SceneManager.h"

class VertexBufferModel;
class AssignedMaterial;
class ShaderConstantsProvider;

struct BoundingBoxDraw
{
	BoundingBoxDraw(GraphicsResources& resources, const std::vector<DXGI_FORMAT>& targets);

	void renderAxisAligned(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider& constants, SceneEntity* entity);

	void renderObjectAligned(ID3D12GraphicsCommandList* commandList, ShaderConstantsProvider& constants, SceneEntity* entity);

private:

	VertexBufferModel* model{};
	AssignedMaterial* material{};

	GraphicsResources& resources;
};

struct PlanesModel
{
	struct PointInfo
	{
		Vector3 startPosition;
		float width;
	};

	void CreatePlanesVertexBuffer(RenderSystem& rs, ResourceUploadBatch& batch, const std::vector<PointInfo>& planes)
	{
		if (model.vertexCount)
			return;

		struct VertexLayout
		{
			Vector3 position;
			Vector2 uv;
			Vector3 normal = Vector3(0,1,0);
		};
		std::vector<VertexLayout> vertices;

		Vector3 lastDirection = planes[1].startPosition - planes[0].startPosition;
		Quaternion rotation = Quaternion::FromToRotation(Vector3::UnitZ, lastDirection);

		float uvScale = 300.f;
		float uvCounter = 0;
		for (size_t i = 0; i < planes.size(); i++)
		{
			auto& plane = planes[i];

			Vector3 offset = rotation * Vector3{ plane.width / 2.0f, 0, 0 };
			Vector3 left = plane.startPosition + offset;
			Vector3 right = plane.startPosition - offset;

			vertices.push_back({ left, { 0.f, uvCounter * uvScale } });
			vertices.push_back({ right, { uvScale, uvCounter * uvScale } });
			uvCounter++;
		}

		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
		model.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);

		model.CreateVertexBuffer(rs.core.device, &batch, vertices.data(), (UINT)vertices.size(), sizeof(VertexLayout));
		model.calculateBounds();
	}

	void AssignToEntity(SceneEntity* e)
	{
		e->geometry.fromModel(model);
		e->geometry.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		e->setBoundingBox(model.bbox);
	}

private:

	VertexBufferModel model{};
};