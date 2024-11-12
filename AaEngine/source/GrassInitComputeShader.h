#pragma once

#include "ComputeShader.h"
#include "DirectXMath.h"
#include "AaMath.h"

struct GrassAreaDescription;

class GrassInitComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, GrassAreaDescription& desc, XMMATRIX invView, UINT colorTex, UINT depthTex, ID3D12Resource* vertexBuffer, ID3D12Resource* vertexCounter, UINT frameIndex);
};