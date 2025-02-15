#pragma once

#include "RenderSystem.h"
#include "GraphicsResources.h"
#include "ComputeShader.h"
#include "EntityGeometry.h"

class SceneManager;

class VegetationFindComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT terrainId, ID3D12Resource* infoBuffer, ID3D12Resource* counter);
};

class VegetationUpdateComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* tranformBuffer, ID3D12Resource* counter, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter);
};

struct VegetationGroup
{
	ComPtr<ID3D12Resource> infoBuffer;
	ComPtr<ID3D12Resource> transformationBuffer;

	ComPtr<ID3D12Resource> infoCounter;

	IndirectEntityGeometry impostors;
};

class Vegetation
{
public:

	Vegetation();

	void initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch);

	void clear();

	SceneEntity* createDrawObject(SceneManager& sceneMgr, RenderSystem& renderSystem, MaterialInstance& material, ResourceUploadBatch& batch, GraphicsResources& resources);

	void update(ID3D12GraphicsCommandList* commandList, UINT terrainId);

private:

	std::vector<std::unique_ptr<VegetationGroup>> vegetations;
	std::vector<VegetationGroup*> scheduled;

	VegetationFindComputeShader vegetationFindCS;
	VegetationUpdateComputeShader vegetationUpdateCS;

	void createDrawObjectImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch, GraphicsResources& resources, IndirectEntityGeometry& chunk);

	void initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch);

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> defaultCommandBuffer;

	void createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch);
	ComPtr<ID3D12Resource> indexBuffer;
};
