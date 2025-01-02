#pragma once

#include "GraphicsResources.h"
#include <string>

class VertexBufferModel;
class AssignedMaterial;
class ShaderConstantsProvider;

struct SceneSkybox
{
	SceneSkybox(GraphicsResources& resources);

	void setMaterial(const std::string& name, const std::vector<DXGI_FORMAT>& targets);

	void render(ID3D12GraphicsCommandList* commandList, const ShaderConstantsProvider& constants);

private:

	VertexBufferModel* model{};
	AssignedMaterial* material{};

	GraphicsResources& resources;
};