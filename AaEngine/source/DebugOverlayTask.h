#pragma once

#include "CompositorTask.h"
#include "ScreenQuad.h"

class AssignedMaterial;

class DebugOverlayTask : public CompositorTask
{
public:

	DebugOverlayTask(RenderProvider, SceneManager&);
	~DebugOverlayTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void resize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	bool writesSyncCommands(CompositorPass&) const override;

	static DebugOverlayTask& Get();
	void changeIdx(int idx);
	int currentIdx() const;
	const char* getCurrentIdxName() const;

private:

	AssignedMaterial* material{};
	ScreenQuad quad;

	int current = -1;
};
