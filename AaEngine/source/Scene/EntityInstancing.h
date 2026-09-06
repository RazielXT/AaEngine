#pragma once

#include "Resources/Shader/ShaderDataBuffers.h"
#include "Scene/RenderObject.h"
#include "Scene/EntityGeometry.h"
#include "Resources/ResourcesView.h"
#include "Scene/InstancePlacement.h"
#include <vector>

class RenderWorld;
class RenderEntity;
class MaterialInstance;
class VertexBufferModel;
class ProgressiveTerrain;
class RenderSystem;
struct GraphicsResources;

struct InstanceGroupDescription
{
	MaterialInstance* material;
	VertexBufferModel* model;
	std::vector<ObjectTransformation> objects;
	// Zero keeps all objects in one sector. Terrain-driven groups should use a
	// power-of-two subdivision of the terrain tile size.
	float sectorWorldSize{};
};

struct InstancedEntity
{
	ObjectTransformation transformation;
	UINT sectorIndex{};
};

struct InstanceSector
{
	XMINT2 worldCoord{};
	bool dirty{};
	ComPtr<ID3D12Resource> gpuBuffer;
	RenderEntity* entity{};
	std::vector<UINT> instanceIndices;
	IndirectEntityGeometry indirect;
};

struct InstanceGroup
{
	InstanceGroup() = default;
	~InstanceGroup();

	void create(RenderWorld&, const InstanceGroupDescription&, UINT groupIdx);
	void createHeightmap(RenderWorld&, RenderSystem&, GraphicsResources&, const HeightmapInstanceGroupDescription&, UINT groupIdx, ComPtr<ID3D12CommandSignature>);
	void update(UINT idx, ObjectTransformation);
	void updateHeightmap(ID3D12GraphicsCommandList*, const Vector3& cameraPosition, const ProgressiveTerrain&);
	void updateBbox();
	RenderEntity* getEntity(UINT instanceIdx) const;
	ComPtr<ID3D12Resource> gpuBuffer;
	ComPtr<ID3D12Resource> gpuIdsBuffer;
	ShaderViewUAV gpuIdsBufferView{};

	UINT count{};
	RenderEntity* entity{};
	std::vector<InstancedEntity> entities;
	std::vector<InstanceSector> sectors;

	BoundingBox modelBbox;
	// Manual updates retain their original sector in the first implementation;
	// its bounds expand as needed, so culling remains conservative.
	float sectorWorldSize{};
	MaterialInstance* material{};
	VertexBufferModel* model{};
	bool heightmapPlaced{};

private:
	void createSectors(RenderWorld&, UINT groupIdx);
	void rebuildSector(UINT sectorIdx);
	void updateSectorBbox(UINT sectorIdx);
	void updateHeightmapSectorPositions(const Vector3&, const ProgressiveTerrain&);
	void regenerateHeightmapSectors(ID3D12GraphicsCommandList*, const ProgressiveTerrain&);

	XMUINT2 sectorGridSize{};
	XMINT2 sectorGridCenter{};
	UINT maxInstancesPerSector{};
	BoundingBox generatedInstanceBounds{};
	ComPtr<ID3D12Resource> generatedInstanceBuffer;
	ComPtr<ID3D12Resource> generatedCommandBuffer;
	std::unique_ptr<HeightmapPlacementComputeShader> placementShader;
	std::vector<UINT> placementConstants;
	bool generatedBuffersInitialized{};
};

class InstancingManager
{
public:

	InstancingManager() = default;

	void initialize(RenderSystem&, GraphicsResources&);
	void update();
	InstanceGroup* build(RenderWorld& renderWorld, const InstanceGroupDescription&);
	InstanceGroup* buildHeightmap(RenderWorld&, const HeightmapInstanceGroupDescription&);
	void updateHeightmap(ID3D12GraphicsCommandList*, const Vector3& cameraPosition, const ProgressiveTerrain&);
	void clear();

	InstanceGroup* getGroup(UINT) const;

	void updateEntity(ObjectId, const ObjectTransformation&);

private:

	std::map<UINT, std::unique_ptr<InstanceGroup>> groups;

	struct ScheduledUpdate
	{
		ObjectId id;
		ObjectTransformation transform;
	};
	std::vector<ScheduledUpdate> updates;

	RenderSystem* renderSystem{};
	GraphicsResources* resources{};
	ComPtr<ID3D12CommandSignature> drawIndexedCommandSignature;
};