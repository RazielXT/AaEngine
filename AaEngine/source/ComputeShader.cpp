#include "ComputeShader.h"
#include "AaShaderLibrary.h"
#include "ShaderSignature.h"

void ComputeShader::init(ID3D12Device* device, const std::string& name)
{
	auto shader = AaShaderLibrary::get().getShader(name, ShaderTypeCompute);

	if (!shader)
		return;

	SignatureInfo info;
	info.add(shader, ShaderTypeCompute);
	info.finish();

	signature = info.createRootSignature(device);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = signature.Get();
	computePsoDesc.CS = { shader->blob->GetBufferPointer(), shader->blob->GetBufferSize() };

	device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pipelineState));
}
