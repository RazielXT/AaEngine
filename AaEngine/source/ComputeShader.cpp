#include "ComputeShader.h"
#include "ShaderLibrary.h"
#include "ShaderSignature.h"
#include "StringUtils.h"

ComputeShader::~ComputeShader()
{
	if (signature)
		signature->Release();
}

void ComputeShader::init(ID3D12Device& device, const std::string& name, ShaderLibrary& shaders)
{
	return init(device, name, *shaders.getShader(name, ShaderType::ShaderTypeCompute));
}

void ComputeShader::init(ID3D12Device& device, const std::string& name, const LoadedShader& shader)
{
	SignatureInfo info;
	info.add(shader, ShaderTypeCompute);
	info.finish();

	if (volatileTextures)
		info.setTexturesVolatile();

	signature = info.createRootSignature(device, as_wstring(name).c_str(), { {.bordering = D3D12_TEXTURE_ADDRESS_MODE_BORDER} });

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = signature;
	computePsoDesc.CS = { shader.blob->GetBufferPointer(), shader.blob->GetBufferSize() };

	device.CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pipelineState));
}
