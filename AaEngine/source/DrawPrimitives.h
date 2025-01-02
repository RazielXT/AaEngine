#pragma once

#include "GraphicsResources.h"
#include <string>

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
