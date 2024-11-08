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

	if (!shader)
		return;

	SignatureInfo info;
	info.add(shader, ShaderTypeCompute);
	info.finish();

	signature = info.createRootSignature(device, std::wstring(name.begin(), name.end()).data(), {{.bordering = D3D12_TEXTURE_ADDRESS_MODE_BORDER}});

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = signature;
	computePsoDesc.CS = { shader->blob->GetBufferPointer(), shader->blob->GetBufferSize() };

	device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pipelineState));
}
