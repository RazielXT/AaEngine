#include "Vegetation.h"
#include "BufferHelpers.h"
#include "SceneManager.h"

Vegetation::Vegetation()
{
}

void Vegetation::initialize(RenderSystem& renderSystem, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	auto csShader = resources.shaders.getShader("vegetationFindCS", ShaderTypeCompute, ShaderRef{ "vegetationFindCS.hlsl", "main", "cs_6_6" });
	vegetationFindCS.init(*renderSystem.core.device, "vegetationFind", *csShader);

	csShader = resources.shaders.getShader("vegetationUpdateCS", ShaderTypeCompute, ShaderRef{ "vegetationUpdateCS.hlsl", "main", "cs_6_6" });
	vegetationUpdateCS.init(*renderSystem.core.device, "vegetationUpdate", *csShader);

	createBillboardIndexBuffer(renderSystem, batch);
	initializeImpostors(renderSystem, batch);
}

void Vegetation::clear()
{
	scheduled.clear();
	vegetations.clear();
}

const UINT groups = 4;
const UINT threads = 8;
const UINT perThread = 8;

struct VegetationInfo
{
	Vector3 position;
	float rotation;
	float scale;
};

SceneEntity* Vegetation::createDrawObject(SceneManager& sceneMgr, RenderSystem& renderSystem, MaterialInstance& material, ResourceUploadBatch& batch, GraphicsResources& resources)
{
	auto& v = vegetations.emplace_back();

	v = std::make_unique<VegetationGroup>();

	constexpr UINT row = groups * threads * perThread;
	constexpr UINT maxCount = row * row;

	constexpr UINT elementInfoSize = sizeof(VegetationInfo);
	constexpr UINT infoSize = elementInfoSize * maxCount;
	v->infoBuffer = resources.shaderBuffers.CreateStructuredBuffer(infoSize, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	v->infoBuffer->SetName(L"VegetationInfo");

	constexpr UINT tranformationInfoSize = sizeof(XMFLOAT4X4);
	constexpr UINT tranformationSize = tranformationInfoSize * maxCount;
	v->transformationBuffer = resources.shaderBuffers.CreateStructuredBuffer(tranformationSize);
	v->transformationBuffer->SetName(L"VegetationTransformation");

	v->infoCounter = resources.shaderBuffers.CreateStructuredBuffer(sizeof(UINT), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	v->infoCounter->SetName(L"VegetationInfoCounter");

	scheduled.push_back(v.get());

	createDrawObjectImpostors(renderSystem, batch, resources, v->impostors);

	auto e = sceneMgr.createEntity("vegetation");
	e->material = &material;
	e->geometry.type = EntityGeometry::Type::Indirect;
	e->geometry.source = &v->impostors;
	e->geometry.geometryCustomBuffer = v->infoBuffer->GetGPUVirtualAddress();

	e->geometry.indexCount = 6;
	e->geometry.indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	e->geometry.indexBufferView.SizeInBytes = e->geometry.indexCount * sizeof(uint16_t);
	e->geometry.indexBufferView.Format = DXGI_FORMAT_R16_UINT;

	e->setBoundingBox({ {}, {} }); //no culling
	e->setFlag(RenderObjectFlag::NoShadow);

	return e;
}

struct IndirectCommand
{
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

void Vegetation::update(ID3D12GraphicsCommandList* commandList, UINT terrainId)
{
	for (auto& s : scheduled)
	{
		vegetationFindCS.dispatch(commandList, terrainId, s->infoBuffer.Get(), s->infoCounter.Get());

		CD3DX12_RESOURCE_BARRIER barrier[2]{};
		barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(s->infoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(s->infoCounter.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(2, barrier);
	}

	scheduled.clear();

	for (auto& v : vegetations)
	{
		CD3DX12_RESOURCE_BARRIER barrier[2]{};
		barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(v->impostors.commandBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, barrier);

		commandList->CopyBufferRegion(v->impostors.commandBuffer.Get(), 0, defaultCommandBuffer.Get(), 0, sizeof(IndirectCommand));
		//commandList->CopyResource(v->impostors.commandBuffer.Get(), defaultCommandBuffer.Get());

		barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(v->impostors.commandBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(v->transformationBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(2, barrier);

		vegetationUpdateCS.dispatch(commandList, v->transformationBuffer.Get(), v->impostors.commandBuffer.Get(), v->infoBuffer.Get(), v->infoCounter.Get());

		barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(v->impostors.commandBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		barrier[1] = CD3DX12_RESOURCE_BARRIER::Transition(v->transformationBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(2, barrier);
	}
}

void Vegetation::createDrawObjectImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch, GraphicsResources& resources, IndirectEntityGeometry& chunk)
{
	chunk.commandSignature = commandSignature;
	chunk.commandBuffer = resources.shaderBuffers.CreateStructuredBuffer(sizeof(IndirectCommand));
	chunk.commandBuffer->SetName(L"VegetationCommandBuffer");

	chunk.maxCommands = 1;
}

void Vegetation::initializeImpostors(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	// Each command consists of a CBV update and a DrawInstanced call.
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

	renderSystem.core.device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&commandSignature));

	IndirectCommand defaultData{};
	defaultData.drawArguments.IndexCountPerInstance = 6; //quad
	defaultData.drawArguments.InstanceCount = 0; //filled by CS

	CreateStaticBuffer(renderSystem.core.device, batch, &defaultData, sizeof(IndirectCommand), D3D12_RESOURCE_STATE_COPY_SOURCE, &defaultCommandBuffer);
}

void VegetationFindComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, UINT terrainId, ID3D12Resource* infoBuffer, ID3D12Resource* counter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct Input
	{
		UINT terrainTexture;
		Vector3 terrainScale;
		Vector2 terrainOffset;
	}
	ctx = { terrainId, Vector3(8192.f, 8000.f, 8192.f), Vector2(8192.f / 2.f)};

	commandList->SetComputeRoot32BitConstants(0, sizeof(ctx) / sizeof(float), &ctx, 0);
	commandList->SetComputeRootUnorderedAccessView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(2, counter->GetGPUVirtualAddress());

	commandList->Dispatch(groups, groups, 1);
}

void VegetationUpdateComputeShader::dispatch(ID3D12GraphicsCommandList* commandList, ID3D12Resource* tranformBuffer, ID3D12Resource* commands, ID3D12Resource* infoBuffer, ID3D12Resource* infoCounter)
{
	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetComputeRootSignature(signature);

	struct
	{
		Vector3 offset;
	}
	ctx = {};

	commandList->SetComputeRoot32BitConstants(0, sizeof(ctx) / sizeof(float), &ctx, 0);
	commandList->SetComputeRootShaderResourceView(1, infoBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootShaderResourceView(2, infoCounter->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(3, tranformBuffer->GetGPUVirtualAddress());
	commandList->SetComputeRootUnorderedAccessView(4, commands->GetGPUVirtualAddress());

	commandList->Dispatch(groups * groups, 1, 1);
}

void Vegetation::createBillboardIndexBuffer(RenderSystem& renderSystem, ResourceUploadBatch& batch)
{
	//quad 2 triangles
	std::vector<uint16_t> data = { 0, 1, 2, 2, 1, 3 };

	CreateStaticBuffer(renderSystem.core.device, batch, data, D3D12_RESOURCE_STATE_INDEX_BUFFER, &indexBuffer);
	indexBuffer->SetName(L"VegetationBillboardIB");
}
