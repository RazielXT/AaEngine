#pragma once

#include "ApplicationCore.h"
#include "imgui.h"
#include <map>
#include <string>

class EditorSelection;
class ImguiPanelViewport;

class ViewportPanel
{
public:

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

	ViewportPanel(ApplicationCore& app, EditorSelection& selection, ImguiPanelViewport& renderPanelViewport, DescriptorHeapAllocator& srvDescHeapAlloc, RenderCore& renderer);

	ObjectTransformation draw(Camera& camera);

	void scheduleViewportPick();
	void resetOutputDescriptor();

	void loadIcons();
	void initializeIconViews();

	bool isActive() const { return active; }
	bool isHovered() const { return hovered; }
	bool isGizmoActive() const { return gizmoActive; }
	bool isOverlayActive() const { return overlayActive; }
	XMUINT2 getViewportSize() const { return viewportPanelSize; }

	void cancelGizmo() { gizmoReset = true; }
	void reset();

	std::map<std::string, FileTexture*>& getIcons() { return icons; }

private:

	ApplicationCore& app;
	EditorSelection& selection;
	ImguiPanelViewport& renderPanelViewport;
	DescriptorHeapAllocator& srvDescHeapAlloc;
	RenderCore& renderer;

	void ensureOutputDescriptor();

	XMUINT2 viewportBounds[2];
	XMUINT2 viewportPanelSize{};
	bool active{};
	bool hovered{};
	bool scenePickScheduled = false;
	bool gizmoActive = false;
	bool gizmoReset = false;
	bool gizmoResetCache = false;
	bool overlayActive = false;
	std::string assetDrop;

	D3D12_GPU_DESCRIPTOR_HANDLE renderOutputHandleGpu{};
	D3D12_CPU_DESCRIPTOR_HANDLE renderOutputHandleCpu{};

	std::map<std::string, FileTexture*> icons;
};
