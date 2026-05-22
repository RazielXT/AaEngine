#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "RenderCore/ScreenQuad.h"
#include "Resources/DescriptorManager.h"

class AssignedMaterial;

struct TexturePreviewData
{
	UINT textureIndex{};
	UINT resId{};
	float remapMin = 0.0f;
	float remapMax = 1.0f;
};

struct TexturePreview3DData
{
	UINT textureIndex{};
	UINT sliceIndex{};
	UINT resId{};
	float remapMin = 0.0f;
	float remapMax = 1.0f;
};

class DebugOverlayTask : public CompositorTask
{
public:

	DebugOverlayTask(RenderProvider, RenderWorld&);
	~DebugOverlayTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	void enable(bool enabled);

	static DebugOverlayTask& Get();
	void changeIdx(UINT idx);
	void setIdx(UINT idx);
	UINT currentIdx() const;
	const DescriptorManager::DescriptorInfo* getCurrentDescriptor() const;
	bool isCurrentTexture3D() const;
	std::vector<DescriptorManager::DescriptorInfo> getTextureList() const;

	bool isFullscreen() const;
	void setFullscreen(bool);

	UINT getSliceIdx() const;
	void setSliceIdx(UINT idx);

	bool isRemapEnabled() const;
	void setRemapEnabled(bool);
	Vector2 getRemapMinMax() const;
	void setRemapMinMax(Vector2);

	RunType getRunType(CompositorPass&) const override { return RunType::SyncCommands; }

private:

	AssignedMaterial* material{};
	AssignedMaterial* material3D{};
	AssignedMaterial* materialUint{};
	AssignedMaterial* material3DUint{};

	Vector2 screenSize{};
	bool fullscreen = false;
	void updateQuad();
	ScreenQuad quad;

	UINT current = 0;
	UINT sliceIdx = 0;
	bool enabled = false;

	bool remapEnabled = false;
	Vector2 remapMinMax = { 0.0f, 1.0f };
};
