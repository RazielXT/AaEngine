#pragma once

#include "ApplicationCore.h"
#include "InputHandler.h"
#include "EditorSelection.h"
#include "DebugState.h"
#include "LogPanel.h"
#include "AssetBrowserPanel.h"
#include "TerrainShaderGraphPanel.h"
#include "SceneTreePanel.h"
#include "SidePanel.h"
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
	void drawViewport(Camera& camera, ObjectTransformation& objTransformation);

	XMUINT2 m_ViewportBounds[2];
	bool scenePickScheduled = false;
	bool ctrlActive = false;
	bool gizmoActive = false;
	bool gizmoReset = false;
	bool gizmoResetCache = false;
	bool overlayActive = false;
	std::string assetDrop;

	EditorSelection selection;

	XMUINT2 viewportPanelSize;
	XMUINT2 lastViewportPanelSize;

	D3D12_GPU_DESCRIPTOR_HANDLE renderOutputTextureImguiHandleGpu{};
	D3D12_CPU_DESCRIPTOR_HANDLE renderOutputTextureImguiHandleCpu{};
	void ensureViewportOutputDescriptor();

	struct DescriptorHeapAllocator
	{
		ID3D12DescriptorHeap* Heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
		D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
		UINT                        HeapHandleIncrement;
		ImVector<int>               FreeIndices;

		void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap);
		void Destroy();
		void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);
		void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle);
	};

	ID3D12DescriptorHeap* srvDescHeap = nullptr;
	DescriptorHeapAllocator srvDescHeapAlloc;
	CommandsData commands;

	std::map<std::string, FileTexture*> icons;
	void loadIcons();
	void initializeIconViews();

	SceneTreePanel sceneTreePanel;
	LogPanel logPanel;
	AssetBrowserPanel assetBrowserPanel;
	TerrainShaderGraphPanel terrainShaderGraphPanel;
	SidePanel sidePanel;
};
