#pragma once

#include "ComputeShader.h"

class ShaderTextureView;

class AoRenderCS : public ComputeShader
{
public:

// 	struct
// 	{
// 		float gInvThicknessTable[12];
// 		float gSampleWeightTable[12];
// 		float gInvSliceDimension[2];
// 		float gRejectFadeoff;
// 		float gRcpAccentuation;
// 	}
// 	data;

	__declspec(align(16)) float SsaoCB[28];

	void dispatch(UINT width, UINT height, UINT arraySize, float TanHalfFovH, const ShaderTextureView& input, const ShaderTextureView& output, ID3D12GraphicsCommandList* commandList, UINT frameIndex);

private:

	void setupData(UINT BufferWidth, UINT BufferHeight, UINT ArrayCount, float TanHalfFovH);
};
