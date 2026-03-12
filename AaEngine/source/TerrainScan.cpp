#include "TerrainScan.h"

void TerrainScanScheduler::scheduleScan(TerrainScanTask task, ID3D12GraphicsCommandList* commandList, const FrameParameters& frame, GraphicsResources& resources, SceneManager& sceneMgr)
{
// 	Camera tmpCamera;
// 	const UINT NearClipDistance = 1;
// 	auto bbox = task.bbox;
// 	tmpCamera.setOrthographicCamera(bbox.Extents.x * 2, bbox.Extents.z * 2, 1, bbox.Extents.y * 2 + NearClipDistance);
// 	tmpCamera.setPosition(bbox.Center + Vector3(0, bbox.Extents.y + NearClipDistance, 0));
// 	tmpCamera.setDirection({ 0, -1, 0 });
// 	tmpCamera.updateMatrix();
// 
// 	RenderObjectsStorage tmpStorage;
// 	SceneEntity entity(tmpStorage, *task.terrain);
// 	RenderObjectsVisibilityData visibility;
// 	tmpStorage.updateVisibility(tmpCamera, visibility);
// 
// 	rtt.PrepareAsTarget(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
// 
// 	ShaderConstantsProvider constants(frame, visibility, tmpCamera, rtt);
// 
// 	RenderQueue queue{ formats, MaterialTechnique::TerrainScan };
// 	queue.update({ EntityChange::Add, Order::Normal, &entity }, resources);
// 	queue.renderObjects(constants, commandList);
// 	//queue.update({ { EntityChange::DeleteAll } });
// 
// 	rtt.TransitionFromTarget(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
// 
// 	GrassAreaDescription desc;
// 	desc.initialize(task.bbox);
// 
// 	auto grassEntity = createGrassEntity("grass_" + std::string(entity.name), desc, sceneMgr);
// 	auto grassArea = createGrassArea(desc, resources);
// 
// 	{
// 		auto colorTex = rtt.textures[0].view.srvHeapIndex;
// 		auto normalTex = rtt.textures[1].view.srvHeapIndex;
// 		auto typeTex = rtt.textures[2].view.srvHeapIndex;
// 		auto depthTex = rtt.textures[3].view.srvHeapIndex; //rtt.depth.view.srvHeapIndex;
// 
// 		grassCS.dispatch(commandList, desc, XMMatrixInverse(nullptr, tmpCamera.getProjectionMatrix()), { colorTex, normalTex, typeTex, depthTex }, grassArea->gpuBuffer.Get(), grassArea->vertexCounter.Get());
// 
// 		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(grassArea->vertexCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
// 		commandList->ResourceBarrier(1, &barrier);
// 		commandList->CopyResource(grassArea->vertexCounterRead.Get(), grassArea->vertexCounter.Get());
// 	}
// 
// 	scheduled.emplace_back(grassEntity, grassArea);
}