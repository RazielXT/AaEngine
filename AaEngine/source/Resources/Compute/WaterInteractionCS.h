#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Utils/MathUtils.h"

class WaterInteractionCS : public ComputeShader
{
public:

	struct DispatchParams
	{
		UINT queryCount;
		UINT waterHeightSrvIndex;
		UINT waterVelocitySrvIndex;
		UINT padding;
		Vector2 worldCenter;
		Vector2 worldSize;
		float waterHeight;
		float waterHeightStart;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, DispatchParams params, ID3D12Resource* inputBuffer, ID3D12Resource* outputBuffer);
};
