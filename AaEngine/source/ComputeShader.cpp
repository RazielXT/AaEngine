#include "ComputeShader.h"
#include "AaShaderLibrary.h"
#include "ShaderSignature.h"

ComputeShader::~ComputeShader()
{
	if (signature)
		signature->Release();
}

void ComputeShader::init(ID3D12Device* device, const std::string& name)
{
	auto shader = AaShaderLibrary::get().getShader(name, ShaderTypeCompute);

	if (shader)
		init(device, name, shader);
}

void ComputeShader::init(ID3D12Device* device, const std::string& name, const ShaderRef& ref)
{
	auto shader = AaShaderLibrary::get().getShader(name, ShaderTypeCompute, ref);

	if (shader)
		init(device, name, shader);
}

void ComputeShader::init(ID3D12Device* device, const std::string& name, LoadedShader* shader)
{
	SignatureInfo info;
	info.add(shader, ShaderTypeCompute);
	info.finish();

	if (volatileTextures)
		info.setTexturesVolatile();

	signature = info.createRootSignature(device, std::wstring(name.begin(), name.end()).data(), { {.bordering = D3D12_TEXTURE_ADDRESS_MODE_BORDER} });

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = signature;
	computePsoDesc.CS = { shader->blob->GetBufferPointer(), shader->blob->GetBufferSize() };

	device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pipelineState));
}
