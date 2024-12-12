#pragma once

#include "d3d12.h"
#include <wrl.h>
#include "RenderTargetTexture.h"

struct TextureResource
{
	~TextureResource();

	void create3D(ID3D12Device* device, UINT width, UINT height, UINT depth, DXGI_FORMAT format);
	void setName(const char* name);
	std::string name;

	static void TransitionState(ID3D12GraphicsCommandList* commandList, TextureResource&, D3D12_RESOURCE_STATES);

	ComPtr<ID3D12Resource> texture;
	std::vector<ShaderUAV> uav;
	ShaderTextureView textureView;

	DXGI_FORMAT format{};
	UINT width{}, height{}, depth{};
	D3D12_RESOURCE_DIMENSION dimension{};
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};