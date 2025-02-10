#pragma once

#include "ApplicationCore.h"
#include "InputHandler.h"

class ImguiPanelViewport : public TargetViewport
{
public:

	void set(HWND hw, UINT w, UINT h)
	{
		hwnd = hw;
		height = h;
		width = w;
	}
};

struct DebugState
{
	bool reloadShaders = false;
	int TexturePreviewIndex = -1;

	const char* changeScene{};

	int DlssMode = (int)UpscaleMode::Off;
	int FsrMode = (int)UpscaleMode::Off;

	bool limitFrameRate = true;
};

class Editor
{
public:

	Editor(ApplicationCore&, ImguiPanelViewport&);

	void initialize(TargetWindow&);

	void updateViewportSize();
	void resetViewportOutput();

	void render(Camera& camera);

	void deinit();

	bool rendererActive{};
	bool rendererHovered{};

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

	XMUINT2 m_ViewportBounds[2];
	Vector3 selectionPosition{};
	bool updateEntitySelect = false;
	bool ctrlActive = false;
	bool addTree = false;
	bool addTreeNormals = false;

	XMUINT2 viewportPanelSize;
	XMUINT2 lastViewportPanelSize;

	D3D12_GPU_DESCRIPTOR_HANDLE renderOutputTextureImguiHandleGpu{};
	D3D12_CPU_DESCRIPTOR_HANDLE renderOutputTextureImguiHandleCpu{};

	struct SelectionInfo
	{
		ObjectTransformation origin;
		SceneObject obj;
	};
	std::vector<SelectionInfo> selection{};

	std::map<std::string, FileTexture*> icons;
	void loadIcons();
	void initializeIconViews();
};
