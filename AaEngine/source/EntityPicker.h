#pragma once

#include "RenderTargetTexture.h"
#include "RenderContext.h"
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

	EntityPicker(RenderSystem& renderSystem);
	~EntityPicker();

	static EntityPicker& Get();

	void initializeGpuResources();
	void update(ID3D12GraphicsCommandList* commandList, RenderProvider& provider, Camera& camera, SceneManager& sceneMgr);

	void scheduleNextPick(XMUINT2 position);

	struct PickInfo
	{
		ObjectId id;
		Vector3 position;
		Vector3 normal;
	};
	const PickInfo& getLastPick();
	bool lastPickWasTransparent() const;

	std::vector<ObjectId> active;

private:

	RenderTargetTextures rtt;
	RenderTargetHeap heap;

	RenderQueue createRenderQueue() const;

	ComPtr<ID3D12Resource> readbackBuffer[3];
	void scheduleReadback(ID3D12GraphicsCommandList* commandList);
	void readPickResult();

	const std::vector<DXGI_FORMAT> formats = { DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
	const UINT formatSize = 4 * sizeof(UINT);

	PickInfo lastPick{};

	std::optional<XMUINT2> scheduled;
	bool nextPickPrepared = false;

	RenderSystem& renderSystem;
};