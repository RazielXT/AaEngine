#pragma once

#include "ComputeShader.h"
#include "DirectXMath.h"
#include "MathUtils.h"

struct GrassAreaDescription;

class GrassInitComputeShader : public ComputeShader
{
public:

	struct InputTextures
	{
		UINT colorTex;
		UINT normalTex;
		UINT typeTex;
		UINT depthTex;
	};
	void dispatch(ID3D12GraphicsCommandList* commandList, GrassAreaDescription& desc, XMMATRIX invView, InputTextures textures, ID3D12Resource* vertexBuffer, ID3D12Resource* vertexCounter);
};