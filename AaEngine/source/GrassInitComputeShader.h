#pragma once

#include "ComputeShader.h"
#include "DirectXMath.h"
#include "AaMath.h"

struct TextureResource;
class ResourcesManager;
struct GrassArea;

class GrassInitComputeShader : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, GrassArea& grass, UINT colorTexture, UINT depthTexture, XMMATRIX invVPMatrix, ResourcesManager& mgr, UINT frameIndex);
};