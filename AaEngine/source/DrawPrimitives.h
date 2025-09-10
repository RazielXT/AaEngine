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

	void CreatePlanesVertexBuffer(RenderSystem& rs, ResourceUploadBatch& batch, const std::vector<PointInfo>& planes, float uvScale, bool worldUvScale = false)
	{
		if (model.vertexCount)
			return;

		struct VertexLayout
		{
			Vector3 position;
			Vector2 uv;
			Vector3 normal;
			Vector3 tangent;
		};
		std::vector<VertexLayout> vertices;
		vertices.reserve(planes.size() * 2);

		Vector3 nextDirection = planes[1].startPosition - planes[0].startPosition;
		nextDirection.Normalize();

		Quaternion rotation = Quaternion::FromToRotation(Vector3::UnitZ, nextDirection);

		float uvCounter = 0;
		for (size_t i = 0; i < planes.size(); i++)
		{
			auto& plane = planes[i];

			Vector3 offset = rotation * Vector3{ plane.width / 2.0f, 0, 0 };
			Vector3 left = plane.startPosition + offset;
			Vector3 right = plane.startPosition - offset;

			float localUvScale = uvScale * (worldUvScale ? plane.width : 1);
			Vector2 uvLeft = { 0.f, uvCounter * localUvScale };
			Vector2 uvRight = { localUvScale, uvCounter * localUvScale };

			Vector3 lastDirection = nextDirection;
			if (i + 1 != planes.size())
			{
				nextDirection = planes[i + 1].startPosition - planes[i].startPosition;
				nextDirection.Normalize();
			}

			// Tangent is the direction from left to right
			Vector3 tangent = Vector3(right - left);
			tangent.Normalize();

			Vector3 normal = tangent.Cross((lastDirection + nextDirection) / 2.f);

			vertices.emplace_back(left, uvLeft, normal, tangent);
			vertices.emplace_back(right, uvRight, normal, tangent);
			uvCounter++;
		}

		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
		model.addLayoutElement(DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
		model.addLayoutElement(DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT);

		model.CreateVertexBuffer(rs.core.device, &batch, vertices.data(), (UINT)vertices.size(), sizeof(VertexLayout));
		model.calculateBounds();
	}

	SceneEntity* CreateEntity(const std::string& name, SceneManager& sceneMgr, MaterialInstance* material)
	{
		auto e = sceneMgr.createEntity(name);
		e->material = material;
		e->geometry.fromModel(model);
		e->geometry.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		e->setBoundingBox(model.bbox);
		e->setTransformation({}, true);

		return e;
	}

private:

	VertexBufferModel model{};
};