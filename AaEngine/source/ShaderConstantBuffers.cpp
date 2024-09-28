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
	constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	const size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);

	auto& c = cbuffers[name];
	c = { graphicsMemory->Allocate(alignedSize, alignment), graphicsMemory->Allocate(alignedSize, alignment) };

	return { &c.data[0], &c.data[1] };
}

CbufferView ShaderConstantBuffers::GetCbufferResource(std::string name)
{
	auto& c = cbuffers[name];
	return { &c.data[0], &c.data[1] };
}
