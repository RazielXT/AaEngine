#pragma once

#include "RenderCore/GpuTexture.h"
#include "FrameCompositor/RenderContext.h"
#include "Resources/Shader/ShaderResources.h"
#include "Resources/GraphicsResources.h"
#include "Scene/ObjectId.h"
#include <vector>

class RenderEntity;
class RenderWorld;
struct RenderQueue;

class EntityPicker
{
public:

	EntityPicker(RenderSystem& renderSystem);
	~EntityPicker();

	static EntityPicker& Get();

	void initializeGpuResources();
	void update(ID3D12GraphicsCommandList* commandList, RenderProvider& provider, Camera& camera, RenderWorld& renderWorld);

	struct PickOptions
	{
		XMUINT2 position;
		bool onlyTransparent = false;
	};
	void scheduleNextPick(PickOptions);

	struct PickInfo
	{
		ObjectId id;
		Vector3 position;
		Vector3 normal;
	};
	bool hasPreparedPick() const;
	PickInfo getLastPick();

	bool lastPickWasTransparent() const;

	std::vector<ObjectId> active;

private:

	RenderTargetTextures rtt;
	RenderTargetHeap heap;

	RenderQueue createRenderQueue() const;

	ComPtr<ID3D12Resource> readbackBuffer[3][FrameCount];

	void scheduleReadback(ID3D12GraphicsCommandList* commandList);
	void readPickResult();

	const std::vector<DXGI_FORMAT> formats = { DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
	const UINT formatSize = 4 * sizeof(UINT);

	PickInfo lastPick{};

	std::optional<PickOptions> scheduled;
	bool nextPickScheduled[FrameCount]{};
	bool nextPickAvailable[FrameCount]{};

	RenderSystem& renderSystem;
};