#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "RenderCore/ScreenQuad.h"
#include "Resources/DescriptorManager.h"

class AssignedMaterial;

struct TexturePreviewData
{
	UINT textureIndex{};
	float remapMin = 0.0f;
	float remapMax = 1.0f;
};

struct TexturePreview3DData
{
	UINT textureIndex{};
	UINT sliceIndex{};
	float remapMin = 0.0f;
	float remapMax = 1.0f;
};

enum class PreviewMode
{
	Preview,	// Positioned at top-right, 50% size
	Fit,		// Native texture size
	Fullscreen  // Full screen
};

class DebugOverlayTask : public CompositorTask
{
public:

	DebugOverlayTask(RenderProvider, RenderWorld&);
	~DebugOverlayTask();

	void initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void recordCommands(RenderContext& ctx, CommandsData& commands, CompositorPass& pass) override;

	void enable(bool enabled);

	static DebugOverlayTask& Get();
	void changeIdx(UINT idx);
	void setIdx(UINT idx);
	UINT currentIdx() const;
	const DescriptorManager::DescriptorInfo* getCurrentDescriptor() const;
	bool isCurrentTexture3D() const;
	std::vector<DescriptorManager::DescriptorInfo> getTextureList() const;

	PreviewMode getPreviewMode() const;
	void setPreviewMode(PreviewMode mode);

	UINT getSliceIdx() const;
	void setSliceIdx(UINT idx);

	bool isRemapEnabled() const;
	void setRemapEnabled(bool);
	Vector2 getRemapMinMax() const;
	void setRemapMinMax(Vector2);

	Execution getExecution(CompositorPass&) const override { return { RecordMode::Inline, Queue::Graphics }; }

private:

	AssignedMaterial* material{};
	AssignedMaterial* material3D{};
	AssignedMaterial* materialUint{};
	AssignedMaterial* material3DUint{};

	Vector2 screenSize{};
	PreviewMode previewMode = PreviewMode::Preview;
	void updateQuad();
	ScreenQuad quad;

	UINT current = 0;
	UINT sliceIdx = 0;
	bool enabled = false;

	bool remapEnabled = false;
	Vector2 remapMinMax = { 0.0f, 1.0f };
};
