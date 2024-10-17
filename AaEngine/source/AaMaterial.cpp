﻿#include "AaMaterial.h"
#include "..\..\DirectX-Headers\include\directx\d3dx12.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <ranges>
#include "AaCamera.h"
#include "AaEntity.h"
#include "AaShaderResources.h"
#include "AaTextureResources.h"
#include "AaLogger.h"
#include <sstream>
#include <functional>

MaterialBase::MaterialBase(AaRenderSystem* rs, ResourcesManager& m, const MaterialRef& matRef) : mgr(m), renderSystem(rs), ref(matRef)
{
}

MaterialBase::~MaterialBase()
{
	for (const auto& s : pipelineStates)
	{
		s.pipeline->Release();
	}

	if (rootSignature)
		rootSignature->Release();
}

void MaterialBase::Load()
{
	if (rootSignature)
		return;

	if (!ref.pipeline.vs_ref.empty())
		shaders[ShaderTypeVertex] = AaShaderResources::get().getShader(ref.pipeline.vs_ref, ShaderTypeVertex);
	if (!ref.pipeline.ps_ref.empty())
		shaders[ShaderTypePixel] = AaShaderResources::get().getShader(ref.pipeline.ps_ref, ShaderTypePixel);

	for (int i = 0; i < ShaderType_COUNT; i++)
		if (shaders[i])
			info.add(shaders[i], (ShaderType)i);

	info.finish();

	CreateRootSignature();
}

void MaterialBase::CreateRootSignature()
{
	std::vector<CD3DX12_ROOT_PARAMETER1> params;
	params.resize(info.cbuffers.size() + info.textures.size());
	auto param = params.data();

	for (auto& buffer : info.cbuffers)
	{
		if (&buffer == info.rootBuffer)
			param->InitAsConstants(buffer.info.Size / sizeof(DWORD), buffer.info.Slot, buffer.info.Space, buffer.visibility);
		else
			param->InitAsConstantBufferView(buffer.info.Slot, buffer.info.Space, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, buffer.visibility);

		param++;
	}

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> srvRanges;
	srvRanges.resize(info.textures.size());
	for (size_t i = 0; i < info.textures.size(); i++)
	{
		auto& tex = info.textures[i];
		auto& range = srvRanges[i];

		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, tex.info.Slot, tex.info.Space, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		param->InitAsDescriptorTable(1, &range, tex.visibility);

		param++;
	}

	std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;
	samplers.resize(info.samplers.size());
	for (size_t i = 0; i < info.samplers.size(); i++)
	{
		auto& sampler = samplers[i];
		sampler.Filter = D3D12_FILTER_ANISOTROPIC;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0.0f;
		sampler.MaxAnisotropy = 8;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = info.samplers[i].visibility;

		if (i < ref.resources.samplers.size())
		{
			auto& refSampler = ref.resources.samplers[i];
			sampler.Filter = refSampler.filter;
			sampler.AddressU = sampler.AddressV = sampler.AddressW = refSampler.bordering;
			sampler.MaxAnisotropy = refSampler.maxAnisotropy;
			sampler.BorderColor = refSampler.borderColor;
		}
	}

	D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	
	if (info.bindlessTextures)
		flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

	if (!shaders[ShaderTypePixel])
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// create the root signature
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(params.size(), params.data(), samplers.size(), samplers.empty() ? nullptr : samplers.data(), flags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);
	renderSystem->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
}

void MaterialBase::BindSignature(ID3D12GraphicsCommandList* commandList, int frameIndex) const
{
	commandList->SetGraphicsRootSignature(rootSignature);

	ID3D12DescriptorHeap* descriptorHeaps[] = { mgr.mainDescriptorHeap[frameIndex] };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

std::size_t HashInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout)
{
	std::size_t seed = 0;
	for (const auto& element : layout)
	{
		std::hash<std::string> hashString;
		seed ^= hashString(element.SemanticName) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

		std::hash<std::uint32_t> hashUint32;
 		seed ^= hashUint32(element.SemanticIndex) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
// 		seed ^= hashUint32(element.Format) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
// 		seed ^= hashUint32(element.AlignedByteOffset) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
	return seed;
}

ID3D12PipelineState* MaterialBase::GetPipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target)
{
	auto layoutHash = HashInputLayout(layout);

	for (const auto& s : pipelineStates)
	{
		if (s.hashId == layoutHash)
			return s.pipeline;
	}

	auto pipelineState = CreatePipelineState(layout, target);

	pipelineStates.emplace_back(layoutHash, layout, target, pipelineState);

	return pipelineState;
}

ID3D12PipelineState* MaterialBase::CreatePipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { layout.empty() ? nullptr : layout.data(), (UINT)layout.size() };
	psoDesc.pRootSignature = rootSignature;
	if (shaders[ShaderTypeVertex])
		psoDesc.VS = { shaders[ShaderTypeVertex]->blob->GetBufferPointer(), shaders[ShaderTypeVertex]->blob->GetBufferSize() };
	if (shaders[ShaderTypePixel])
		psoDesc.PS = { shaders[ShaderTypePixel]->blob->GetBufferPointer(), shaders[ShaderTypePixel]->blob->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	if (!shaders[ShaderTypePixel] || target.empty())
	{
		psoDesc.NumRenderTargets = 0;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	}
	else
	{
		psoDesc.NumRenderTargets = info.textureTargets;

		for (size_t i = 0; i < target.size(); i++)
			psoDesc.RTVFormats[i] = target[i];
	}
	psoDesc.SampleDesc.Count = 1;

	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = ref.pipeline.depth.check;
	if (!ref.pipeline.depth.write)
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	ID3D12PipelineState* pipelineState{};
	auto hr = renderSystem->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(hr)) {
		AaLogger::logErrorD3D("Failed CreateGraphicsPipelineState", hr);
	}

	return pipelineState;
}

AaMaterial* MaterialBase::GetAssignedMaterial(MaterialInstance* instance, const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target)
{
	auto& mats = assignedMaterials[instance];

	auto pipeline = GetPipelineState(layout, target);

	for (auto& m : mats)
	{
		if (*m == pipeline)
			return m.get();
	}

	auto newMat = std::make_unique<AaMaterial>(*instance, pipeline);
	auto ptr = newMat.get();
	mats.emplace_back(std::move(newMat));

	return ptr;
}

void MaterialBase::ReloadPipeline()
{
	std::map<void*, ID3D12PipelineState*> oldMapping;

	for (auto& s : pipelineStates)
	{
		auto newPipeline = CreatePipelineState(s.layout, s.target);
		oldMapping[s.pipeline] = newPipeline;
		s.pipeline->Release();
		s.pipeline = newPipeline;
	}

	for (auto& [instance, materials] : assignedMaterials)
	{
		for (auto& material : materials)
		{
			material->pipelineState = oldMapping[material->pipelineState];
		}
	}
}

bool MaterialBase::ContainsShader(const LoadedShader* shader) const
{
	for (auto s : shaders)
	{
		if (s == shader)
			return true;
	}

	return false;
}

std::unique_ptr<MaterialInstance> MaterialBase::CreateMaterialInstance(const MaterialRef& childRef, ResourceUploadBatch& batch)
{
	Load();

	auto instance = std::make_unique<MaterialInstance>(*this, childRef);
	instance->resources = CreateResourcesData(childRef);

	UINT texSlot = 0;
	for (const auto& t : childRef.resources.textures)
	{
		if (!t.file.empty())
		{
			auto texture = AaTextureResources::get().loadFile(renderSystem->device, batch, t.file);
			mgr.createShaderResourceView(renderSystem->device, *texture);

			instance->SetTexture(*texture, texSlot);
		}
		else if (!t.id.empty())
		{
			if (auto texture = AaTextureResources::get().getNamedTexture(t.id))
			{
				instance->SetTexture(*texture, texSlot);
			}
		}
		texSlot++;
	}

	return std::move(instance);
}

static void SetDefaultParameter(const CBufferInfo::Parameter& param, const MaterialRef& ref, std::vector<float>& data)
{
	auto it = ref.resources.defaultParams.find(param.Name);
	if (it != ref.resources.defaultParams.end())
	{
		memcpy(data.data() + param.StartOffset / sizeof(float), it->second.data(), it->second.size() * sizeof(float));
	}
}

std::shared_ptr<ResourcesInfo> MaterialBase::CreateResourcesData(const MaterialRef& childRef) const
{
	auto resources = std::make_shared<ResourcesInfo>();
	UINT rootIndex = 0;
	UINT bindlessTextures = 0;

	for (auto& cb : info.cbuffers)
	{
		ResourcesInfo::CBuffer r;
		r.defaultData.resize(cb.info.Size / sizeof(float));

		for (const auto& p : cb.info.Params)
		{
			if (&cb != info.rootBuffer)
				break;

			ResourcesInfo::AutoParam type = ResourcesInfo::AutoParam::None;
			if (p.Name == "WorldViewProjectionMatrix")
				type = ResourcesInfo::AutoParam::WVP_MATRIX;
			else if (p.Name == "ViewProjectionMatrix")
				type = ResourcesInfo::AutoParam::VP_MATRIX;
			else if (p.Name == "WorldMatrix")
				type = ResourcesInfo::AutoParam::WORLD_MATRIX;
			else if (p.Name == "ShadowMatrix")
				type = ResourcesInfo::AutoParam::SHADOW_MATRIX;
			else if (p.Name.starts_with("TexId"))
			{
				type = ResourcesInfo::AutoParam::TEXID;
				bindlessTextures++;
			}
			else if (p.Name == "Time")
				type = ResourcesInfo::AutoParam::TIME;
			else if (p.Name == "ViewportSizeInverse")
				type = ResourcesInfo::AutoParam::VIEWPORT_SIZE_INV;
			else if (p.Name == "SunDirection")
				type = ResourcesInfo::AutoParam::SUN_DIRECTION;

			if (type != ResourcesInfo::AutoParam::None)
				resources->autoParams.emplace_back(type, rootIndex, (UINT)(p.StartOffset / sizeof(float)));
			else
			{
				SetDefaultParameter(p, ref, r.defaultData);
				SetDefaultParameter(p, childRef, r.defaultData);
			}
		}

		if (&cb == info.rootBuffer)
			r.type = CBufferType::Root;
		else if (cb.info.Name == "Instancing")
			r.type = CBufferType::Instancing;
		else
			r.globalBuffer = ShaderConstantBuffers::get().GetCbufferResource(cb.info.Name);

		r.rootIndex = rootIndex++;
		resources->cbuffers.emplace_back(std::move(r));
	}

	resources->textures.resize(info.textures.size() + bindlessTextures);

	for (size_t i = 0; i < info.textures.size(); i++)
	{
		resources->textures[i].rootIndex = rootIndex++;
	}

	return resources;
}

MaterialInstance::MaterialInstance(MaterialBase& matBase, const MaterialRef& matRef) : base(matBase), ref(matRef)
{
}

MaterialInstance::~MaterialInstance()
{

}

AaMaterial* MaterialInstance::Assign(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target)
{
	return base.GetAssignedMaterial(this, layout, target);
}

const MaterialBase* MaterialInstance::GetBase() const
{
	return &base;
}

bool MaterialInstance::HasInstancing() const
{
	for (auto& r : resources->cbuffers)
	{
		if (r.type == CBufferType::Instancing)
			return true;
	}
	return false;
}

void MaterialInstance::SetTexture(ShaderTextureView& texture, UINT slot)
{
	auto& t = resources->textures[slot];
	t.texture = &texture;

	if (t.rootIndex == BindlessTextureIndex)
		UpdateBindlessTexture(texture, slot);
}

ShaderTextureView* MaterialInstance::GetTexture(UINT slot) const
{
	return resources->textures[slot].texture;
}

void MaterialInstance::SetParameter(const std::string& name, float* value, size_t size)
{
	int idx = 0;
	for (auto& b : base.info.cbuffers)
	{
		for (auto& p : b.info.Params)
		{
			if (p.Name == name)
			{
				memcpy(resources->cbuffers[idx].defaultData.data() + p.StartOffset / sizeof(float), value, size * sizeof(float));
				break;
			}
		}
		idx++;
	}
}

void MaterialInstance::LoadMaterialConstants(MaterialConstantBuffers& buffers) const
{
	if (buffers.data.size() < resources->cbuffers.size())
		buffers.data.resize(resources->cbuffers.size());

	for (const auto& p : resources->cbuffers)
	{
		if (p.type == CBufferType::Root)
			buffers.data[p.rootIndex] = p.defaultData;
	}
}

void MaterialInstance::UpdatePerFrame(MaterialConstantBuffers& buffers, const FrameGpuParameters& info, const XMMATRIX& vpMatrix)
{
	for (auto p : resources->autoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::TIME)
			buffers.data[p.bufferIdx][p.bufferOffset] = info.time;
		else if (p.type == ResourcesInfo::AutoParam::VIEWPORT_SIZE_INV)
		{
			buffers.data[p.bufferIdx][p.bufferOffset] = info.inverseViewportSize.x;
			buffers.data[p.bufferIdx][p.bufferOffset + 1] = info.inverseViewportSize.y;
		}
		else if (p.type == ResourcesInfo::AutoParam::SUN_DIRECTION)
			*(DirectX::XMFLOAT3*)&buffers.data[p.bufferIdx][p.bufferOffset] = info.sunDirection;
		else if (p.type == ResourcesInfo::AutoParam::VP_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&buffers.data[p.bufferIdx][p.bufferOffset], XMMatrixTranspose(vpMatrix));
	}
}

void MaterialInstance::UpdatePerObject(MaterialConstantBuffers& buffers, const XMFLOAT4X4& wvpMatrix, const XMMATRIX& worldMatrix, const FrameGpuParameters& info)
{
	for (auto p : resources->autoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::WVP_MATRIX)
			*(DirectX::XMFLOAT4X4*)&buffers.data[p.bufferIdx][p.bufferOffset] = wvpMatrix;
		else if (p.type == ResourcesInfo::AutoParam::WORLD_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&buffers.data[p.bufferIdx][p.bufferOffset], XMMatrixTranspose(worldMatrix));
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MATRIX)
			*(DirectX::XMFLOAT4X4*)&buffers.data[p.bufferIdx][p.bufferOffset] = info.shadowMapViewProjectionTransposed;
	}
}

void MaterialInstance::UpdateBindlessTexture(const ShaderTextureView& texture, UINT slot)
{
	UINT textureIdx = base.info.textures.size(); //offset after bound textures
	for (auto p : resources->autoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::TEXID && textureIdx++ == slot)
		{
			auto& buff = resources->cbuffers[p.bufferIdx].defaultData;
			*(UINT*)&buff[p.bufferOffset] = texture.srvHeapIndex;
			break;
		}
	}
}

void MaterialInstance::BindTextures(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
	for (auto& t : resources->textures)
		if (t.texture && t.rootIndex != BindlessTextureIndex)
			commandList->SetGraphicsRootDescriptorTable(t.rootIndex, t.texture->srvHandles[frameIndex]);
}

void MaterialInstance::BindConstants(ID3D12GraphicsCommandList* commandList, int frameIndex, const MaterialConstantBuffers& buffers)
{
	for (auto& cb : resources->cbuffers)
	{
		if (cb.type == CBufferType::Root)
			commandList->SetGraphicsRoot32BitConstants(cb.rootIndex, buffers.data[cb.rootIndex].size(), buffers.data[cb.rootIndex].data(), 0);
		else if (cb.type == CBufferType::Instancing)
			commandList->SetGraphicsRootConstantBufferView(cb.rootIndex, buffers.cbuffers.instancing.data[frameIndex]->GpuAddress());
		else
 			commandList->SetGraphicsRootConstantBufferView(cb.rootIndex, cb.globalBuffer.data[frameIndex]->GpuAddress());
	}
}

void AaMaterial::BindPipeline(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState);
}
