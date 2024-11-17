#include "AoBlurAndUpsampleCS.h"
#include "DescriptorManager.h"
#include "DebugWindow.h"

void AoBlurAndUpsampleCS::dispatch(UINT fullWidth, UINT highHeight, UINT highWidth, UINT lowHeight, UINT lowWidth, ID3D12GraphicsCommandList* commandList, UINT frameIndex)
{
	const float g_BlurTolerance = imgui::DebugWindow::state.ssao.BlurTolerance;
	const float g_UpsampleTolerance = imgui::DebugWindow::state.ssao.UpsampleTolerance;
	const float g_NoiseFilterTolerance = imgui::DebugWindow::state.ssao.NoiseFilterTolerance;

	float kBlurTolerance = 1.0f - powf(10.0f, g_BlurTolerance) * fullWidth / (float)lowWidth;
	kBlurTolerance *= kBlurTolerance;
	float kUpsampleTolerance = powf(10.0f, g_UpsampleTolerance);
	float kNoiseFilterWeight = 1.0f / (powf(10.0f, g_NoiseFilterTolerance) + kUpsampleTolerance);

	data.InvLowResolution[0] = 1.0f / lowWidth;
	data.InvLowResolution[1] = 1.0f / lowHeight;
	data.InvHighResolution[0] = 1.0f / highWidth;
	data.InvHighResolution[1] = 1.0f / highHeight;
	data.NoiseFilterStrength = kNoiseFilterWeight;
	data.StepSize = fullWidth / (float)lowWidth;
	data.kBlurTolerance = kBlurTolerance;
	data.kUpsampleTolerance = kUpsampleTolerance;

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	ID3D12DescriptorHeap* ppHeaps[] = { DescriptorManager::get().mainDescriptorHeap[frameIndex] };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	commandList->SetComputeRoot32BitConstants(0, sizeof(data) / sizeof(float), &data, 0);

	const uint32_t bufferWidth = (highWidth + 15) / 16;
	const uint32_t bufferHeight = (highHeight + 15) / 16;

	commandList->Dispatch(bufferWidth, bufferHeight, 1);
}
