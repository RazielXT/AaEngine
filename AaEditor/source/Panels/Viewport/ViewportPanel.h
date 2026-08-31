#pragma once

#include "ApplicationCore.h"
#include "ViewportPlacement.h"
#include "imgui.h"
#include <map>
#include <string>
#include <memory>
#include <optional>
#include <unordered_set>
#include "SelectionTool.h"
#include "InputHandler.h"

class EditorSelection;
class ImguiPanelViewport;
class ViewportTool;
class SelectionTool;
class SplineRoadTool;

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

	void draw(Camera& camera);

	void scheduleViewportPick(bool transparent = false);
	void resetOutputDescriptor();

	bool isActive() const { return active; }
	bool isHovered() const { return hovered; }
	XMUINT2 getViewportSize() const { return viewportPanelSize; }

	void setActiveTool(ViewportTool* tool);
	ViewportTool* getActiveTool() const { return activeTool; }
	void registerSplineRoadTool(SplineRoadTool* tool) { splineRoadTool = tool; }
	void beginPlacement(ViewportPlacementRequest request);
	void placeAtCursor(ViewportPlacementRequest request);
	void cancelPlacement();
	bool isPlacingFor(const ViewportTool* owner) const;

	bool onClick(MouseButton);
	void reset();

private:

	ApplicationCore& app;
	EditorSelection& selection;
	ImguiPanelViewport& renderPanelViewport;
	DescriptorHeapAllocator& srvDescHeapAlloc;
	RenderCore& renderer;

	void ensureOutputDescriptor();
	void schedulePlacementPick();
	void drawPlacementHint() const;

	XMUINT2 viewportBounds[2];
	XMUINT2 viewportPanelSize{};
	bool active{};
	bool hovered{};

	ViewportTool* activeTool = nullptr;
	std::unique_ptr<SelectionTool> selectionTool;
 	SplineRoadTool* splineRoadTool = nullptr;
	std::optional<ViewportPlacementRequest> placement;
	uint64_t placementPickRequestId = 0;
	std::unordered_set<uint64_t> placementPickRequestIds;

	D3D12_GPU_DESCRIPTOR_HANDLE renderOutputHandleGpu{};
	D3D12_CPU_DESCRIPTOR_HANDLE renderOutputHandleCpu{};
};
