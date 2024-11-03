#include "ShaderConstantBuffers.h"
#include "AaRenderSystem.h"

static ShaderConstantBuffers* instance = nullptr;

ShaderConstantBuffers::ShaderConstantBuffers(AaRenderSystem* rs)
{
	renderSystem = rs;
	graphicsMemory = std::make_unique<DirectX::DX12::GraphicsMemory>(renderSystem->device);

	if (instance)
		throw std::exception("Duplicate ShaderConstantBuffers");

	instance = this;
}

ShaderConstantBuffers::~ShaderConstantBuffers()
{
	instance = nullptr;
}

ShaderConstantBuffers& ShaderConstantBuffers::get()
{
	return *instance;
}

CbufferView ShaderConstantBuffers::CreateCbufferResource(UINT size, std::string name)
{
	auto& c = cbuffers[name];
	c = CreateCbufferResource(size);

	return c;
}

CbufferData ShaderConstantBuffers::CreateCbufferResource(UINT size)
{
	constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	const size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);

	return { graphicsMemory->Allocate(alignedSize, alignment), graphicsMemory->Allocate(alignedSize, alignment) };
}

CbufferView ShaderConstantBuffers::GetCbufferResource(std::string name)
{
	auto& c = cbuffers[name];
	return c;
}

ComPtr<ID3D12Resource> ShaderConstantBuffers::CreateStructuredBuffer(const void* data, UINT dataSize)
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = dataSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES props = {};
	props.Type = D3D12_HEAP_TYPE_UPLOAD;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ComPtr<ID3D12Resource> buffer;
	renderSystem->device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

	D3D12_RANGE readRange = {};
	void* pData = nullptr;
	buffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));
	memcpy(pData, data, dataSize);
	buffer->Unmap(0, nullptr);

	return buffer;
}

CbufferView::CbufferView(CbufferData& buffer)
{
	data[0] = &buffer.data[0];
	data[1] = &buffer.data[1];
}
