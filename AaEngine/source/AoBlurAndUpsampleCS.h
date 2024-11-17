#pragma once

#include "ComputeShader.h"

class AoBlurAndUpsampleCS : public ComputeShader
{
public:

	struct
	{
		float InvLowResolution[2];
		float InvHighResolution[2];
		float NoiseFilterStrength;
		float StepSize;
		float kBlurTolerance;
		float kUpsampleTolerance;

		UINT ResIdLoResDB;
		UINT ResIdHiResDB;
		UINT ResIdLoResAO1;
		UINT ResIdLoResAO2;
		UINT ResIdHiResAO;
		UINT ResIdAoResult;
	}
	data;

	void dispatch(UINT fullWidth, UINT highHeight, UINT highWidth, UINT lowHeight, UINT lowWidth, ID3D12GraphicsCommandList* commandList, UINT frameIndex);
};
