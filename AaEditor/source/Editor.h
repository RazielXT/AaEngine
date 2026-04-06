#pragma once

#include "ApplicationCore.h"
#include "InputHandler.h"
#include "imgui.h"

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

	const char* changeScene{};

	int DlssMode = (int)UpscaleMode::Off;
	int FsrMode = (int)UpscaleMode::Off;

	bool limitFrameRate = false;

	bool wireframe = false;
	bool wireframeChange = false;
};

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

	bool rendererActive{};
	bool rendererHovered{};

	bool onClick(MouseButton);
	bool keyPressed(int key);
	bool keyReleased(int key);

	void reset();

	DebugState state;

private:

	void scheduleViewportPick();

	RenderCore& renderer;
	ApplicationCore& app;
	ImguiPanelViewport& renderPanelViewport;

	void prepareElements(Camera& camera);

	XMUINT2 m_ViewportBounds[2];
	Vector3 selectionPosition{};
	bool scenePickScheduled = false;
	bool ctrlActive = false;
	std::string assetDrop;

	void selectItem(ObjectId, bool multi);
	void deleteSelectedItem();
	void deleteItem(SceneGraphNode& node);

	void refreshSelectionId();
	void clearSelection();

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
	std::vector<ObjectId> selectionIds{};

	std::map<std::string, FileTexture*> icons;
	void loadIcons();
	void initializeIconViews();

	void DrawSceneTree();
	void DrawSceneTreeNode(SceneGraphNode& node, ImGuiTextFilter& filter, ObjectId& selectedObjectId);

	enum class AssetType { Model, Scene, Prefab };

	struct AssetItem
	{
		std::string name;
		std::string path;
		AssetType type;
	};
	std::vector<AssetItem> assets;

	void lookupAssets();
	void DrawAssetBrowser(const std::vector<AssetItem>& assets);

	struct AssetBrowserFilter
	{
		char search[128] = "";
		bool showModel = true;
		bool showScene = true;
		bool showPrefab = true;
	}
	assetsFilter;

	bool PassesFilter(const AssetItem& asset, const AssetBrowserFilter& filter);

	void DrawAssetBrowserTopBar(AssetBrowserFilter& filter);
};
