#pragma once

#include "d3d12.h"
#include <wrl.h>
#include "RenderTargetTexture.h"

struct TextureResource
{
	void create3D(ID3D12Device* device, UINT width, UINT height, UINT depth, DXGI_FORMAT format, int frameCount);

	ComPtr<ID3D12Resource> textures[2];
	ShaderUAV uav;
	ShaderTextureView textureView;

	DXGI_FORMAT format{};
	UINT width{}, height{}, depth{};
	D3D12_RESOURCE_DIMENSION dimension{};
};