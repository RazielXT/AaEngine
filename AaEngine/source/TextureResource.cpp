#include "TextureResource.h"
#include "directx\d3dx12.h"
#include "DescriptorManager.h"

TextureResource::~TextureResource()
{
	DescriptorManager::get().removeTextureView(*this);
	DescriptorManager::get().removeUAVView(*this);
}

void TextureResource::create3D(ID3D12Device* device, UINT w, UINT h, UINT d, DXGI_FORMAT f)
{
	format = f;
	width = w;
	height = h;
	depth = d;
	dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = dimension;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = depth;
	textureDesc.MipLevels = 0;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&texture));
}

void TextureResource::setName(const char* n)
{
	name = n;
	texture->SetName(std::wstring(name.begin(), name.end()).c_str());
}

void TextureResource::TransitionState(ID3D12GraphicsCommandList* commandList, TextureResource& t, D3D12_RESOURCE_STATES targetState)
{
	auto b = CD3DX12_RESOURCE_BARRIER::Transition(t.texture.Get(), t.state, targetState);
	commandList->ResourceBarrier(1, &b);
	t.state = targetState;
}
