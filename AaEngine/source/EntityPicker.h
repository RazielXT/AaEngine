#pragma once

#include "RenderTargetTexture.h"
#include "ShaderResources.h"
#include "GraphicsResources.h"
#include "ObjectId.h"
#include <vector>

class SceneEntity;
class SceneManager;
struct RenderQueue;

class EntityPicker
{
public:

	EntityPicker();
	~EntityPicker();

	static EntityPicker& Get();

	void initializeGpuResources(RenderSystem& renderSystem, GraphicsResources& resources);
	void clear();

	void renderEntityIds(ID3D12GraphicsCommandList* commandList, RenderQueue& queue, Camera& camera, const RenderObjectsVisibilityData& info, const FrameParameters& frame);
	void scheduleEntityIdRead(ID3D12GraphicsCommandList* commandList);

	ObjectId readEntityId();
	bool lastPickWasTransparent() const;

	RenderQueue createRenderQueue() const;

	std::optional<XMUINT2> scheduled;

	std::vector<ObjectId> active;

//private:

	RenderTargetHeap heap;
	RenderTargetTextures rtt;
	ComPtr<ID3D12Resource> readbackBuffer;

	const std::vector<DXGI_FORMAT> formats = { DXGI_FORMAT_R32_UINT };

	ObjectId lastPick{};
};