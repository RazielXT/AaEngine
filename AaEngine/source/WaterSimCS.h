#pragma once

#include "ComputeShader.h"
#include "MathUtils.h"

class WaterSimComputeShader : public ComputeShader
{
public:

	struct Textures
	{
		UINT heighMapId;
		UINT waterMapId;
		UINT velocityMapId;
		UINT outWaterMapId;
		UINT outVelocityMapId;
	};
	void dispatch(ID3D12GraphicsCommandList* commandList, UINT gridSize, float dt, Textures);
};
