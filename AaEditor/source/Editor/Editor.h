#pragma once

#include "ApplicationCore.h"
#include "InputHandler.h"
#include "EditorSelection.h"
#include "DebugState.h"
#include "ImguiPanelViewport.h"
#include "Panels/LogPanel.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/TerrainShaderGraphPanel.h"
#include "Panels/SceneTreePanel.h"
#include "Panels/SidePanel.h"
#include "Panels/Viewport/ViewportPanel.h"
#include "imgui.h"

class Editor
{
public:

	Editor(ApplicationCore&, ImguiPanelViewport&);

	void initializeUi(const TargetWindow&);
	void initializeRenderer(DXGI_FORMAT);

	void updateViewportSize();
	void resetViewportOutput();

	void render(Camera& camera);

	void deinit();

	bool isRendererActive() const { return viewportPanel.isActive(); }
	bool isRendererHovered() const { return viewportPanel.isHovered(); }

	bool onClick(MouseButton);
	bool keyPressed(int key);
	bool keyReleased(int key);

	void reset();

	DebugState state;

private:

	RenderCore& renderer;
	ApplicationCore& app;
	ImguiPanelViewport& renderPanelViewport;

	void prepareElements(Camera& camera);

	EditorSelection selection;

	XMUINT2 lastViewportPanelSize;

	ID3D12DescriptorHeap* srvDescHeap = nullptr;
	ViewportPanel::DescriptorHeapAllocator srvDescHeapAlloc;
	CommandsData commands;

	ViewportPanel viewportPanel;
	SceneTreePanel sceneTreePanel;
	LogPanel logPanel;
	AssetBrowserPanel assetBrowserPanel;
	TerrainShaderGraphPanel terrainShaderGraphPanel;
	SidePanel sidePanel;
};
