#include "AoRenderCS.h"
#include "DescriptorManager.h"
#include "DebugWindow.h"

void AoRenderCS::dispatch(UINT width, UINT height, UINT arraySize, float TanHalfFovH, const ShaderTextureView& input, const ShaderTextureView& output, ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	ID3D12DescriptorHeap* ppHeaps[] = { DescriptorManager::get().mainDescriptorHeap };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	setupData(width, height, arraySize, TanHalfFovH);
	commandList->SetComputeRoot32BitConstants(0, sizeof(SsaoCB) / sizeof(float), &SsaoCB, 0);
	commandList->SetComputeRootDescriptorTable(1, input.srvHandles);
	commandList->SetComputeRootDescriptorTable(2, output.uavHandles);

	const UINT threadSize = arraySize == 1 ? 16 : 8;
	const uint32_t bufferWidth = (width + threadSize - 1) / threadSize;
	const uint32_t bufferHeight = (height + threadSize - 1) / threadSize;

	commandList->Dispatch(bufferWidth, bufferHeight, arraySize);
}

void AoRenderCS::setupData(UINT BufferWidth, UINT BufferHeight, UINT ArrayCount, float TanHalfFovH)
{
	// Here we compute multipliers that convert the center depth value into (the reciprocal of)
	// sphere thicknesses at each sample location.  This assumes a maximum sample radius of 5
	// units, but since a sphere has no thickness at its extent, we don't need to sample that far
	// out.  Only samples whole integer offsets with distance less than 25 are used.  This means
	// that there is no sample at (3, 4) because its distance is exactly 25 (and has a thickness of 0.)

	// The shaders are set up to sample a circular region within a 5-pixel radius.
	const float ScreenspaceDiameter = 10.0f;

	// SphereDiameter = CenterDepth * ThicknessMultiplier.  This will compute the thickness of a sphere centered
	// at a specific depth.  The ellipsoid scale can stretch a sphere into an ellipsoid, which changes the
	// characteristics of the AO.
	// TanHalfFovH:  Radius of sphere in depth units if its center lies at Z = 1
	// ScreenspaceDiameter:  Diameter of sample sphere in pixel units
	// ScreenspaceDiameter / BufferWidth:  Ratio of the screen width that the sphere actually covers
	// Note about the "2.0f * ":  Diameter = 2 * Radius

	//const float TanHalfFovH = 1.0f;// / ProjMat[0];

	float ThicknessMultiplier = 2.0f * TanHalfFovH * ScreenspaceDiameter / BufferWidth;

	if (ArrayCount == 1)
		ThicknessMultiplier *= 2.0f;

	// This will transform a depth value from [0, thickness] to [0, 1].
	float InverseRangeFactor = 1.0f / ThicknessMultiplier;

	const float SampleThickness[12]
	{
		sqrtf(1.0f - 0.2f * 0.2f),
		sqrtf(1.0f - 0.4f * 0.4f),
		sqrtf(1.0f - 0.6f * 0.6f),
		sqrtf(1.0f - 0.8f * 0.8f),
		sqrtf(1.0f - 0.2f * 0.2f - 0.2f * 0.2f),
		sqrtf(1.0f - 0.2f * 0.2f - 0.4f * 0.4f),
		sqrtf(1.0f - 0.2f * 0.2f - 0.6f * 0.6f),
		sqrtf(1.0f - 0.2f * 0.2f - 0.8f * 0.8f),
		sqrtf(1.0f - 0.4f * 0.4f - 0.4f * 0.4f),
		sqrtf(1.0f - 0.4f * 0.4f - 0.6f * 0.6f),
		sqrtf(1.0f - 0.4f * 0.4f - 0.8f * 0.8f),
		sqrtf(1.0f - 0.6f * 0.6f - 0.6f * 0.6f)
	};

	// The thicknesses are smaller for all off-center samples of the sphere.  Compute thicknesses relative
	// to the center sample.
	SsaoCB[0] = InverseRangeFactor / SampleThickness[0];
	SsaoCB[1] = InverseRangeFactor / SampleThickness[1];
	SsaoCB[2] = InverseRangeFactor / SampleThickness[2];
	SsaoCB[3] = InverseRangeFactor / SampleThickness[3];
	SsaoCB[4] = InverseRangeFactor / SampleThickness[4];
	SsaoCB[5] = InverseRangeFactor / SampleThickness[5];
	SsaoCB[6] = InverseRangeFactor / SampleThickness[6];
	SsaoCB[7] = InverseRangeFactor / SampleThickness[7];
	SsaoCB[8] = InverseRangeFactor / SampleThickness[8];
	SsaoCB[9] = InverseRangeFactor / SampleThickness[9];
	SsaoCB[10] = InverseRangeFactor / SampleThickness[10];
	SsaoCB[11] = InverseRangeFactor / SampleThickness[11];

	// These are the weights that are multiplied against the samples because not all samples are
	// equally important.  The farther the sample is from the center location, the less they matter.
	// We use the thickness of the sphere to determine the weight.  The scalars in front are the number
	// of samples with this weight because we sum the samples together before multiplying by the weight,
	// so as an aggregate all of those samples matter more.  After generating this table, the weights
	// are normalized.
	SsaoCB[12] = 4.0f * SampleThickness[0];	// Axial
	SsaoCB[13] = 4.0f * SampleThickness[1];	// Axial
	SsaoCB[14] = 4.0f * SampleThickness[2];	// Axial
	SsaoCB[15] = 4.0f * SampleThickness[3];	// Axial
	SsaoCB[16] = 4.0f * SampleThickness[4];	// Diagonal
	SsaoCB[17] = 8.0f * SampleThickness[5];	// L-shaped
	SsaoCB[18] = 8.0f * SampleThickness[6];	// L-shaped
	SsaoCB[19] = 8.0f * SampleThickness[7];	// L-shaped
	SsaoCB[20] = 4.0f * SampleThickness[8];	// Diagonal
	SsaoCB[21] = 8.0f * SampleThickness[9];	// L-shaped
	SsaoCB[22] = 8.0f * SampleThickness[10];	// L-shaped
	SsaoCB[23] = 4.0f * SampleThickness[11];	// Diagonal

	//#define SAMPLE_EXHAUSTIVELY

			// If we aren't using all of the samples, delete their weights before we normalize.
#ifndef SAMPLE_EXHAUSTIVELY
	SsaoCB[12] = 0.0f;
	SsaoCB[14] = 0.0f;
	SsaoCB[17] = 0.0f;
	SsaoCB[19] = 0.0f;
	SsaoCB[21] = 0.0f;
#endif

	// Normalize the weights by dividing by the sum of all weights
	float totalWeight = 0.0f;
	for (int i = 12; i < 24; ++i)
		totalWeight += SsaoCB[i];
	for (int i = 12; i < 24; ++i)
		SsaoCB[i] /= totalWeight;

	// Controls how aggressive to fade off samples that occlude spheres but by so much as to be unreliable.
	// This is what gives objects a dark halo around them when placed in front of a wall.  If you want to
	// fade off the halo, boost your rejection falloff.  The tradeoff is that it reduces overall AO.
	const float RejectionFalloff = imgui::DebugWindow::state.ssao.RejectionFalloff;// 1.0f - 10.0f

	// The effect normally marks anything that's 50% occluded or less as "fully unoccluded".  This throws away
	// half of our result.  Accentuation gives more range to the effect, but it will darken all AO values in the
	// process.  It will also cause "under occluded" geometry to appear to be highlighted.  If your ambient light
	// is determined by the surface normal (such as with IBL), you might not want this side effect.
	const float Accentuation = imgui::DebugWindow::state.ssao.Accentuation; //0.0f - 1.0f

	SsaoCB[24] = 1.0f / BufferWidth;
	SsaoCB[25] = 1.0f / BufferHeight;
	SsaoCB[26] = 1.0f / -RejectionFalloff;
	SsaoCB[27] = 1.0f / (1.0f + Accentuation);
}
