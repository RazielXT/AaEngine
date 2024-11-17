#include "AaMaterial.h"
#include "..\..\DirectX-Headers\include\directx\d3dx12.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <ranges>
#include "AaCamera.h"
#include "AaEntity.h"
#include "AaShaderLibrary.h"
#include "AaTextureResources.h"
#include "AaLogger.h"
#include <sstream>
#include <functional>
#include <CommonStates.h>

MaterialBase::MaterialBase(AaRenderSystem* rs, DescriptorManager& m, const MaterialRef& matRef) : mgr(m), renderSystem(rs), ref(matRef)
{
}

MaterialBase::~MaterialBase()
{
	for (const auto& s : pipelineStates)
	{
		if (s.pipeline)
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
		shaders[ShaderTypeVertex] = AaShaderLibrary::get().getShader(ref.pipeline.vs_ref, ShaderTypeVertex);
	if (!ref.pipeline.ps_ref.empty())
		shaders[ShaderTypePixel] = AaShaderLibrary::get().getShader(ref.pipeline.ps_ref, ShaderTypePixel);

	for (int i = 0; i < ShaderType_COUNT; i++)
		if (shaders[i])
			info.add(shaders[i], (ShaderType)i);

	info.finish();

	rootSignature = info.createRootSignature(renderSystem->device, std::wstring(ref.name.begin(), ref.name.end()).data(), ref.resources.samplers);
}

void MaterialBase::BindSignature(ID3D12GraphicsCommandList* commandList, int frameIndex) const
{
	commandList->SetGraphicsRootSignature(rootSignature);
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
	psoDesc.RasterizerState.CullMode = ref.pipeline.culling;
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
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

	if (!ref.pipeline.depth.check)
	{
		psoDesc.DepthStencilState.DepthEnable = ref.pipeline.depth.check;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_NONE;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	}
	if (!ref.pipeline.depth.write)
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	
	if (ref.pipeline.blend.alphaBlend)
		psoDesc.BlendState = CommonStates::NonPremultiplied;

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
	if (rootSignature)
	{
		rootSignature->Release();
		rootSignature = nullptr;
		info = {};
	}

	Load();

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
	CreateResourcesData(*instance, batch);

	return std::move(instance);
}

const char* MaterialBase::GetTechniqueOverride(MaterialTechnique technique) const
{
	const auto& m = ref.techniqueMaterial[int(technique)];

	return m ? m->c_str() : nullptr;
}

void MaterialInstance::SetFastTableParameter(const std::string& name, float* data, UINT size, UINT offset)
{
	for (size_t i = 0; i < FastParamNames.size(); i++)
	{
		if (name == FastParamNames[i])
			paramsTable[i] = { data, size, offset / UINT(sizeof(float))};
	}
}

static void SetDefaultParameter(const ShaderReflection::CBuffer::Parameter& param, const MaterialRef& ref, std::vector<float>& data)
{
	auto it = ref.resources.defaultParams.find(param.Name);
	if (it != ref.resources.defaultParams.end())
	{
		memcpy(data.data() + param.StartOffset / sizeof(float), it->second.data(), it->second.size() * sizeof(float));
	}
}

void MaterialBase::CreateResourcesData(MaterialInstance& instance, ResourceUploadBatch& batch) const
{
	instance.resources = info.createResourcesData();

	if (info.rootBuffer)
	{
		auto& buffer = instance.resources->rootBuffer;
		for (const auto& p : info.rootBuffer->info.Params)
		{
			SetDefaultParameter(p, ref, buffer.defaultData);
			SetDefaultParameter(p, instance.ref, buffer.defaultData);
			instance.SetFastTableParameter(p.Name, buffer.defaultData.data() + p.StartOffset / sizeof(float), p.Size, p.StartOffset);
		}
	}

	UINT texSlot = 0;
	for (const auto& t : instance.ref.resources.textures)
	{
		if (!t.file.empty())
		{
			auto texture = AaTextureResources::get().loadFile(renderSystem->device, batch, t.file);
			mgr.createTextureView(*texture);

			instance.SetTexture(*texture, texSlot);
		}
		else if (!t.id.empty())
		{
			if (auto texture = AaTextureResources::get().getNamedTexture(t.id))
			{
				instance.SetTexture(*texture, texSlot);
			}
		}
		texSlot++;
	}
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
	for (auto& r : resources->buffers)
	{
		if (r.type == GpuBufferType::Instancing)
			return true;
	}
	return false;
}

bool MaterialInstance::IsTransparent() const
{
	return ref.pipeline.blend.alphaBlend;
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
	if (base.info.rootBuffer)
	{
		for (const auto& p : base.info.rootBuffer->info.Params)
		{
			if (p.Name == name)
			{
				memcpy(resources->rootBuffer.defaultData.data() + p.StartOffset / sizeof(float), value, size * sizeof(float));
				break;
			}
		}
	}
}

void MaterialInstance::SetParameter(FastParam id, float* value)
{
	auto& param = paramsTable[(int)id];
	if (param.data)
	{
		memcpy(param.data, value, param.Size);
	}
}

void MaterialInstance::GetParameter(const std::string& name, float* output) const
{
	if (base.info.rootBuffer)
	{
		for (auto& p : base.info.rootBuffer->info.Params)
		{
			if (p.Name == name)
			{
				memcpy(output, resources->rootBuffer.defaultData.data() + p.StartOffset / sizeof(float), p.Size);
				return;
			}
		}
	}
}

void MaterialInstance::GetParameter(FastParam id, float* output) const
{
	auto& param = paramsTable[(int)id];
	if (param.data)
	{
		memcpy(output, param.data, param.Size);
	}
}

UINT MaterialInstance::GetParameterOffset(FastParam id) const
{
	return paramsTable[(int)id].Offset;
}

void MaterialInstance::LoadMaterialConstants(MaterialDataStorage& data) const
{
	data.rootParams = resources->rootBuffer.defaultData;
}

void MaterialInstance::UpdatePerFrame(MaterialDataStorage& data, const ShaderConstantsProvider& info)
{
	for (auto p : resources->autoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::TIME)
			data.rootParams[p.bufferOffset] = info.params.time;
		else if (p.type == ResourcesInfo::AutoParam::SUN_DIRECTION)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.params.sun.SunDirection;
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MATRIX)
			*(DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset] = info.params.sun.ShadowMatrix[0];
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MAP_SIZE)
			data.rootParams[p.bufferOffset] = info.params.sun.ShadowMapSize;
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MAP_SIZE_INV)
			data.rootParams[p.bufferOffset] = info.params.sun.ShadowMapSizeInv;
		else if (p.type == ResourcesInfo::AutoParam::VIEWPORT_SIZE_INV)
		{
			data.rootParams[p.bufferOffset] = info.inverseViewportSize.x;
			data.rootParams[p.bufferOffset + 1] = info.inverseViewportSize.y;
		}
		else if (p.type == ResourcesInfo::AutoParam::CAMERA_POSITION)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.getCameraPosition();
		else if (p.type == ResourcesInfo::AutoParam::WORLD_POSITION)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.getWorldPosition();
		else if (p.type == ResourcesInfo::AutoParam::VP_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getViewProjectionMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::INV_VP_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getInverseViewProjectionMatrix()));
	}
}

void MaterialInstance::UpdatePerObject(MaterialDataStorage& data, const ShaderConstantsProvider& info)
{
	for (auto p : resources->autoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::WVP_MATRIX)
			*(DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset] = info.getWvpMatrix();
		else if (p.type == ResourcesInfo::AutoParam::WORLD_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getWorldMatrix()));
	}
}

void MaterialInstance::UpdateBindlessTexture(const ShaderTextureView& texture, UINT slot)
{
	UINT textureIdx = base.info.textures.size(); //offset after bound textures
	for (auto p : resources->autoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::TEXID && textureIdx++ == slot)
		{
			auto& buff = resources->rootBuffer.defaultData;
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

	for (auto& uav : resources->uavs)
		commandList->SetGraphicsRootDescriptorTable(uav.rootIndex, uav.uav->uavHandles[frameIndex]);
}

void MaterialInstance::BindConstants(ID3D12GraphicsCommandList* commandList, int frameIndex, const MaterialDataStorage& data, const ShaderConstantsProvider& constants)
{
	if (data.rootParams.size())
		commandList->SetGraphicsRoot32BitConstants(resources->rootBuffer.rootIndex, data.rootParams.size(), data.rootParams.data(), 0);

	for (auto& b : resources->buffers)
	{
		if (b.type == GpuBufferType::Instancing || b.type == GpuBufferType::Geometry)
			commandList->SetGraphicsRootShaderResourceView(b.rootIndex, constants.getGeometryBuffer());
		else if (b.type == GpuBufferType::Global)
 			commandList->SetGraphicsRootConstantBufferView(b.rootIndex, b.globalCBuffer.data[frameIndex]->GpuAddress());
	}
}

void AaMaterial::BindPipeline(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState);
}
