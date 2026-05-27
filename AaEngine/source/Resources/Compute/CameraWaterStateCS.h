#pragma once

#include "Resources/Compute/ComputeShader.h"
#include "Utils/MathUtils.h"

class CameraWaterStateCS : public ComputeShader
{
public:

	struct DispatchParams
	{
		UINT waterHeightSrvIndex;
		float dt;
		Vector2 worldCenter;
		Vector2 worldSize;
		Vector3 cameraPosition;
		float waterHeightScale;
		float waterHeightStart;
		float dryingSpeed;
		UINT resetState;
	};

	void dispatch(ID3D12GraphicsCommandList* commandList, const DispatchParams& params, ID3D12Resource* stateBuffer);
};