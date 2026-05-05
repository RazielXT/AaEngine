#include "ViewportPanel.h"
#include "Editor/EditorSelection.h"
#include "Editor/ImguiPanelViewport.h"
#include "ViewportTool.h"
#include "SelectionTool.h"
#include "imgui.h"
#include "Editor/EntityPicker.h"

void ViewportPanel::DescriptorHeapAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
{
	IM_ASSERT(Heap == nullptr && FreeIndices.empty());
	Heap = heap;
	D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
	HeapType = desc.Type;
	HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
	HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
	HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
	FreeIndices.reserve((int)desc.NumDescriptors);
	for (int n = desc.NumDescriptors; n > 0; n--)
		FreeIndices.push_back(n);
}

void ViewportPanel::DescriptorHeapAllocator::Destroy()
{
	Heap = nullptr;
	FreeIndices.clear();
}

void ViewportPanel::DescriptorHeapAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
{
	IM_ASSERT(FreeIndices.Size > 0);
	int idx = FreeIndices.back();
	FreeIndices.pop_back();
	out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
	out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
}

void ViewportPanel::DescriptorHeapAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
{
	int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
	int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
	IM_ASSERT(cpu_idx == gpu_idx);
	FreeIndices.push_back(cpu_idx);
}

ViewportPanel::ViewportPanel(ApplicationCore& a, EditorSelection& sel, ImguiPanelViewport& rpv, DescriptorHeapAllocator& alloc, RenderCore& r)
	: app(a), selection(sel), renderPanelViewport(rpv), srvDescHeapAlloc(alloc), renderer(r)
{
	selectionTool = std::make_unique<SelectionTool>(a, sel);
	activeTool = selectionTool.get();
}

void ViewportPanel::scheduleViewportPick(bool transparent)
{
	auto [mx, my] = ImGui::GetMousePos();
	mx -= viewportBounds[0].x;
	my -= viewportBounds[0].y;
	XMUINT2 viewportSize = { viewportBounds[1].x - viewportBounds[0].x, viewportBounds[1].y - viewportBounds[0].y };
	my = viewportSize.y - my;
	auto mouseX = (UINT)mx;
	auto mouseY = (UINT)my;

	if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
	{
		EntityPicker::Get().scheduleNextPick({ { mouseX, viewportSize.y - mouseY }, transparent });
	}
}

void ViewportPanel::resetOutputDescriptor()
{
	if (renderOutputHandleGpu.ptr)
	{
		srvDescHeapAlloc.Free(renderOutputHandleCpu, renderOutputHandleGpu);

		renderOutputHandleGpu = {};
		renderOutputHandleCpu = {};
	}
}

bool ViewportPanel::onClick(MouseButton button)
{
	if (button == MouseButton::Right)
	{
		if (activeTool && activeTool->isInteracting())
		{
			activeTool->cancel();
			return true;
		}
	}

	if (activeTool && (activeTool->isInteracting() || activeTool->isOverlayActive()))
		return false;

	if (button == MouseButton::Left)
	{
		scheduleViewportPick();

		return true;
	}

	return false;
}

void ViewportPanel::reset()
{
	if (activeTool)
		activeTool->reset();
}

void ViewportPanel::setActiveTool(ViewportTool* tool)
{
	activeTool = tool ? tool : selectionTool.get();
}

void ViewportPanel::ensureOutputDescriptor()
{
	if (!renderOutputHandleGpu.ptr)
	{
		auto renderOutput = app.compositor->getTexture("Output");
		srvDescHeapAlloc.Alloc(&renderOutputHandleCpu, &renderOutputHandleGpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = renderOutput->texture->GetDesc().Format;

		renderer.device->CreateShaderResourceView(renderOutput->texture.Get(), &srvDesc, renderOutputHandleCpu);
	}
}

void ViewportPanel::draw(Camera& camera)
{
	ObjectTransformation objTransformation{};

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoDecoration);

	auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	auto viewportOffset = ImGui::GetWindowPos();
	viewportBounds[0] = { UINT(viewportMinRegion.x + viewportOffset.x), UINT(viewportMinRegion.y + viewportOffset.y) };
	viewportBounds[1] = { UINT(viewportMaxRegion.x + viewportOffset.x), UINT(viewportMaxRegion.y + viewportOffset.y) };

	auto viewportContent = ImGui::GetContentRegionAvail();
	viewportPanelSize = { (UINT)viewportContent.x, (UINT)viewportContent.y };

	hovered = ImGui::IsWindowHovered();
	active = hovered || ImGui::IsWindowFocused();

	ensureOutputDescriptor();

	ImGui::Image(ImTextureID(renderOutputHandleGpu.ptr), ImVec2{ (float)renderPanelViewport.getWidth(), (float)renderPanelViewport.getHeight() });

	bool ctrlActive = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

	if (EntityPicker::Get().hasPreparedPick())
	{
		auto pickInfo = EntityPicker::Get().getLastPick();
		activeTool->onPick(pickInfo, ctrlActive, camera);
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MODEL"))
		{
			selectionTool->setAssetDrop((const char*)payload->Data);
			setActiveTool(selectionTool.get());
			scheduleViewportPick();
		}

		ImGui::EndDragDropTarget();
	}

	// on top of viewport - always draw selection tool mode icon
	ImGui::SetCursorPosY(ImGui::GetCursorStartPos().y + 5);
	ImGui::SetCursorPosX(ImGui::GetCursorStartPos().x + 5);

	if (selectionTool->drawModeIcon(activeTool == selectionTool.get()))
		setActiveTool(selectionTool.get());

	// active tool draws its overlay/gizmo
	ViewportToolContext ctx{ { viewportBounds[0], viewportBounds[1] }, camera };
	activeTool->draw(ctx);

	ImGui::End();
	ImGui::PopStyleVar();
}
