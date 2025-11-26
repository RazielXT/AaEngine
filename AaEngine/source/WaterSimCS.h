#pragma once

#include "ComputeShader.h"
#include "MathUtils.h"

struct WaterSimTextures
{
	UINT heighMapId;
	UINT waterMapId;
	UINT velocityMapId;
	UINT outWaterMapId;
	UINT outVelocityMapId;
};

class WaterSimMomentumComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT gridSize, float dt, float spacing, WaterSimTextures);
};

class WaterSimContinuityComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, UINT gridSize, float dt, float spacing, WaterSimTextures);
};

class WaterMeshTextureCS : public ComputeShader
{
public:

	struct InputParams
	{
		XMUINT2 gridSize;
		UINT ResIdHeightMap;
		UINT ResIdWaterMap;
		UINT ResIdVelocityMap;
		UINT ResIdColorMap;
	};
	void dispatch(ID3D12GraphicsCommandList* commandList, InputParams);
};
