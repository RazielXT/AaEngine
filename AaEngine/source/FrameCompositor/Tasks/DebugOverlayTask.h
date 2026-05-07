#pragma once

#include "FrameCompositor/Tasks/CompositorTask.h"
#include "RenderCore/ScreenQuad.h"
#include "Resources/DescriptorManager.h"

class AssignedMaterial;

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
	std::vector<DescriptorManager::DescriptorInfo> getTexture2DList() const;

	bool isFullscreen() const;
	void setFullscreen(bool);

	RunType getRunType(CompositorPass&) const override { return RunType::SyncCommands; }

private:

	AssignedMaterial* material{};

	Vector2 screenSize{};
	bool fullscreen = false;
	void updateQuad();
	ScreenQuad quad;

	UINT current = 0;
	bool enabled = false;
};
