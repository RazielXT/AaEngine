#include "Material.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <ranges>
#include "Camera.h"
#include "SceneEntity.h"
#include "ShaderLibrary.h"
#include "TextureResources.h"
#include "FileLogger.h"
#include <sstream>
#include <functional>
#include <CommonStates.h>
#include "StringUtils.h"
#include "GraphicsResources.h"
#include "directx\d3dx12.h"
#include "Directories.h"

MaterialBase::MaterialBase(ID3D12Device& d, const MaterialRef& matRef) : device(d), ref(matRef)
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

void MaterialBase::Load(ShaderLibrary& shaderLib)
{
	if (rootSignature)
		return;

	if (!ref.pipeline.vs_ref.empty())
		shaders[(int)ShaderType::Vertex] = shaderLib.getShader(ref.pipeline.vs_ref, ShaderType::Vertex);
	if (!ref.pipeline.ps_ref.empty())
		shaders[(int)ShaderType::Pixel] = shaderLib.getShader(ref.pipeline.ps_ref, ShaderType::Pixel);

	for (int i = 0; i < (int)ShaderType::COUNT; i++)
		if (shaders[i])
			info.add(*shaders[i], (ShaderType)i);

	info.finish();

	rootSignature = info.createRootSignature(device, as_wstring(ref.name).c_str(), ref.resources.samplers);
}

void MaterialBase::BindSignature(ID3D12GraphicsCommandList* commandList) const
{
	commandList->SetGraphicsRootSignature(rootSignature);
}

ID3D12RootSignature* MaterialBase::GetSignature() const
{
	return rootSignature;
}

std::size_t HashInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, D3D12_COMPARISON_FUNC comparisonFunc)
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
	for (const auto& t : target)
	{
		seed ^= t + (seed << 6) + (seed >> 2);
	}
	seed ^= comparisonFunc * 0x9e3779b9 + (seed << 6) + (seed >> 2);

	return seed;
}

ID3D12PipelineState* MaterialBase::GetPipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, MaterialTechnique technique)
{
	TechniqueProperties props;
	props.comparisonFunc = technique != MaterialTechnique::DepthShadowmap ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
	props.slopeScaledDepthBias = technique == MaterialTechnique::DepthShadowmap ? 1.0f : ref.pipeline.slopeScaledDepthBias;

	auto layoutHash = HashInputLayout(layout, target, props.comparisonFunc);

	for (const auto& s : pipelineStates)
	{
		if (s.hashId == layoutHash)
			return s.pipeline;
	}

	auto pipelineState = CreatePipelineState(layout, target, props);

	pipelineStates.emplace_back(layoutHash, layout, target, props, pipelineState);

	return pipelineState;
}

ID3D12PipelineState* MaterialBase::CreatePipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, const TechniqueProperties& technique)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { layout.empty() ? nullptr : layout.data(), (UINT)layout.size() };
	psoDesc.pRootSignature = rootSignature;
	if (shaders[(int)ShaderType::Vertex])
		psoDesc.VS = { shaders[(int)ShaderType::Vertex]->blob->GetBufferPointer(), shaders[(int)ShaderType::Vertex]->blob->GetBufferSize() };
	if (shaders[(int)ShaderType::Pixel])
		psoDesc.PS = { shaders[(int)ShaderType::Pixel]->blob->GetBufferPointer(), shaders[(int)ShaderType::Pixel]->blob->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = ref.pipeline.culling;
	psoDesc.RasterizerState.FillMode = ref.pipeline.fill;
	psoDesc.RasterizerState.DepthBias = ref.pipeline.depthBias;
	psoDesc.RasterizerState.SlopeScaledDepthBias = technique.slopeScaledDepthBias;

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	if (!shaders[(int)ShaderType::Pixel] || target.empty())
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
	psoDesc.DepthStencilState.DepthFunc = technique.comparisonFunc;

	if (!ref.pipeline.depth.check)
	{
		psoDesc.DepthStencilState.DepthEnable = ref.pipeline.depth.check;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_NONE;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	}
	if (!ref.pipeline.depth.write || ref.pipeline.blend.alphaBlend)
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	
	if (ref.pipeline.blend.alphaBlend)
		psoDesc.BlendState = CommonStates::NonPremultiplied;

	ID3D12PipelineState* pipelineState{};
	auto hr = device.CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(hr)) {
		FileLogger::logErrorD3D("Failed CreateGraphicsPipelineState", hr);
	}

	return pipelineState;
}

AssignedMaterial* MaterialBase::GetAssignedMaterial(MaterialInstance* instance, const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, MaterialTechnique technique)
{
	auto& mats = assignedMaterials[instance];

	auto pipeline = GetPipelineState(layout, target, technique);

	for (auto& m : mats)
	{
		if (*m == pipeline)
			return m.get();
	}

	auto newMat = std::make_unique<AssignedMaterial>(*instance, pipeline);
	newMat->origin = instance;
	auto ptr = newMat.get();
	mats.emplace_back(std::move(newMat));

	return ptr;
}

void MaterialBase::ReloadPipeline(ShaderLibrary& shaderLib)
{
	if (rootSignature)
	{
		rootSignature->Release();
		rootSignature = nullptr;
		info = {};
	}

	Load(shaderLib);

	std::map<void*, ID3D12PipelineState*> oldMapping;

	for (auto& s : pipelineStates)
	{
		auto newPipeline = CreatePipelineState(s.layout, s.target, s.properties);
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

std::unique_ptr<MaterialInstance> MaterialBase::CreateMaterialInstance(const MaterialRef& childRef, GraphicsResources& resources, ResourceUploadBatch& batch)
{
	Load(resources.shaders);

	auto instance = std::make_unique<MaterialInstance>(*this, childRef);
	CreateResourcesData(*instance, resources, batch);

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

void MaterialBase::CreateResourcesData(MaterialInstance& instance, GraphicsResources& resources, ResourceUploadBatch& batch) const
{
	instance.resources = info.createResourcesData(resources);

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
			auto texture = resources.textures.loadFile(device, batch, t.file);
			resources.descriptors.createTextureView(*texture);

			instance.SetTexture(*texture, texSlot);
		}
		else if (!t.id.empty())
		{
			if (auto texture = resources.textures.getNamedTexture(t.id))
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

AssignedMaterial* MaterialInstance::Assign(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, MaterialTechnique technique)
{
	return base.GetAssignedMaterial(this, layout, target, technique);
}

const MaterialBase* MaterialInstance::GetBase() const
{
	return &base;
}

const char* MaterialInstance::GetTechniqueOverride(MaterialTechnique technique) const
{
	const auto& m = ref.techniqueMaterial[int(technique)];

	if (m)
		return m->c_str();

	if (auto m = base.GetTechniqueOverride(technique))
		return m;

	if (technique == MaterialTechnique::Depth || technique == MaterialTechnique::DepthShadowmap)
		return HasInstancing() ? "DepthInstancing" : "Depth";
	else if (technique == MaterialTechnique::Voxelize)
		return HasInstancing() ? "VoxelizeInstancing" : "Voxelize";
	else if (technique == MaterialTechnique::EntityId)
		return HasInstancing() ? "EntityIdInstancing" : (ref.pipeline.culling == D3D12_CULL_MODE_NONE ? "EntityIdCullNone" : "EntityId");

	return nullptr;
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
		UpdateBindlessTexture(t);
}

void MaterialInstance::SetTexture(ShaderTextureView& texture, const std::string& name)
{
	for (UINT slot = 0; const auto& t : ref.resources.textures)
	{
		if (t.id == name)
			SetTexture(texture, slot);

		slot++;
	}
}

ShaderTextureView* MaterialInstance::GetTexture(UINT slot) const
{
	return resources->textures[slot].texture;
}

void MaterialInstance::SetUAV(ID3D12Resource* uav, UINT slot)
{
	auto& u = resources->uavs[slot];
	u.uav = uav;
}

std::unique_ptr<MaterialPropertiesOverride> MaterialInstance::CreateParameterOverride(const MaterialPropertiesOverrideDescription& description) const
{
	auto output = std::make_unique<MaterialPropertiesOverride>();

	if (base.info.rootBuffer)
	{
		for (auto& d : description.params)
		{
			AppendParameterOverride(*output, d.name, d.value, d.sizeBytes);
		}
	}

	return output->params.empty() ? nullptr : std::move(output);
}

void MaterialInstance::AppendParameterOverride(MaterialPropertiesOverride& output, const std::string& name, const void* value, size_t sizeBytes) const
{
	for (const auto& p : base.info.rootBuffer->info.Params)
	{
		if (p.Name == name)
		{
			auto& param = output.params.emplace_back();
			param.offsetFloats = p.StartOffset / sizeof(float);
			param.sizeBytes = sizeBytes;
			memcpy(output.params.back().value, value, param.sizeBytes);
			break;
		}
	}
}

void MaterialInstance::AppendParameterOverride(MaterialPropertiesOverride& output, const std::string& name, MaterialInstance& source, float defaultValue) const
{
	auto& param = output.params.emplace_back();

	for (auto& p : base.info.rootBuffer->info.Params)
	{
		if (p.Name == name)
		{
			param.offsetFloats = p.StartOffset / sizeof(float);
			param.sizeBytes = p.Size;

			if (!source.GetParameter(name, param.value))
			{
				for (auto& d : param.value)
					d = defaultValue;
			}

			break;
		}
	}
}

void MaterialInstance::ApplyParametersOverride(MaterialPropertiesOverride& data, MaterialDataStorage& output) const
{
	for (auto& p : data.params)
	{
		memcpy(output.rootParams.data() + p.offsetFloats, p.value, p.sizeBytes);
	}
}

void MaterialInstance::SetParameter(const std::string& name, const void* value, size_t count)
{
	SetParameter(name, resources->rootBuffer.defaultData.data(), value, count);
}

void MaterialInstance::SetParameter(const std::string& name, float* output, const void* value, size_t count) const
{
	if (base.info.rootBuffer)
	{
		for (const auto& p : base.info.rootBuffer->info.Params)
		{
			if (p.Name == name)
			{
				memcpy(output + p.StartOffset / sizeof(float), value, count * sizeof(float));
				break;
			}
		}
	}
}

void MaterialInstance::SetParameter(FastParam id, const void* value)
{
	auto& param = paramsTable[(int)id];
	if (param.data)
	{
		memcpy(param.data, value, param.Size);
	}
}

void MaterialInstance::SetParameter(ResourcesInfo::AutoParam type, const void* value, size_t count)
{
	for (auto p : resources->objectAutoParams)
	{
		if (p.type == type)
		{
			memcpy(&resources->rootBuffer.defaultData[p.bufferOffset], value, count * sizeof(float));
			return;
		}
	}
}

void MaterialInstance::SetParameter(ResourcesInfo::AutoParam type, const void* value, size_t count, MaterialDataStorage& data)
{
	for (auto p : resources->objectAutoParams)
	{
		if (p.type == type)
		{
			memcpy(&data.rootParams[p.bufferOffset], value, count * sizeof(float));
			return;
		}
	}
}

void MaterialInstance::SetParameter(FastParam id, const void* value, MaterialDataStorage& data)
{
	auto& param = paramsTable[(int)id];
	if (param.data)
	{
		memcpy(data.rootParams.data() + param.Offset, value, param.Size);
	}
}

bool MaterialInstance::GetParameter(const std::string& name, float* output) const
{
	if (base.info.rootBuffer)
	{
		for (auto& p : base.info.rootBuffer->info.Params)
		{
			if (p.Name == name)
			{
				memcpy(output, resources->rootBuffer.defaultData.data() + p.StartOffset / sizeof(float), p.Size);
				return true;
			}
		}
	}

	return false;
}

void MaterialInstance::GetParameter(FastParam id, float* output) const
{
	auto& param = paramsTable[(int)id];
	if (param.data)
	{
		memcpy(output, param.data, param.Size);
	}
}

void MaterialInstance::CopyParameter(FastParam id, MaterialInstance& source, MaterialDataStorage& data, float defaultValue)
{
	auto& targetInfo = paramsTable[(int)id];
	auto& sourceInfo = source.paramsTable[(int)id];

	if (sourceInfo.data)
	{
		memcpy(data.rootParams.data() + targetInfo.Offset, sourceInfo.data, sourceInfo.Size);
	}
	else
	{
		for (UINT i = 0; i < targetInfo.Size / sizeof(float); i++)
			data.rootParams[i + targetInfo.Offset] = defaultValue;
	}
}

void MaterialInstance::LoadMaterialConstants(MaterialDataStorage& data) const
{
	data.rootParams = resources->rootBuffer.defaultData;
}

void MaterialInstance::UpdatePerFrame(MaterialDataStorage& data, const ShaderConstantsProvider& info)
{
	for (auto p : resources->frameAutoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::TIME)
			data.rootParams[p.bufferOffset] = info.params.time;
		else if (p.type == ResourcesInfo::AutoParam::DELTA_TIME)
			data.rootParams[p.bufferOffset] = info.params.timeDelta;
		else if (p.type == ResourcesInfo::AutoParam::SUN_DIRECTION)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.params.sun.SunDirection;
		else if (p.type == ResourcesInfo::AutoParam::SUN_COLOR)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.params.sun.SunColor;
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MATRIX)
			*(DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset] = info.params.sun.ShadowMatrix[1];
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MAP_SIZE)
			data.rootParams[p.bufferOffset] = info.params.sun.ShadowMapSize;
		else if (p.type == ResourcesInfo::AutoParam::SHADOW_MAP_SIZE_INV)
			data.rootParams[p.bufferOffset] = info.params.sun.ShadowMapSizeInv;
		else if (p.type == ResourcesInfo::AutoParam::VIEWPORT_SIZE_INV)
		{
			data.rootParams[p.bufferOffset] = info.inverseViewportSize.x;
			data.rootParams[p.bufferOffset + 1] = info.inverseViewportSize.y;
		}
		else if (p.type == ResourcesInfo::AutoParam::VIEWPORT_SIZE)
			*(DirectX::XMUINT2*)&data.rootParams[p.bufferOffset] = info.viewportSize;
		else if (p.type == ResourcesInfo::AutoParam::CAMERA_POSITION)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.getMainCameraPosition();
		else if (p.type == ResourcesInfo::AutoParam::VP_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getViewProjectionMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::VIEW_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getViewMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::PROJ_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getProjectionMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::INV_VP_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getInverseViewProjectionMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::INV_VIEW_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getInverseViewMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::INV_PROJ_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getInverseProjectionMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::WVP_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixMultiplyTranspose(info.getWorldMatrix(), info.getViewProjectionMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::Z_MAGIC)
			data.rootParams[p.bufferOffset] = info.camera.getCameraZ();
	}

	//TODO: just when texture descriptor changes
	{
		for (UINT i = resources->boundTexturesCount; i < resources->textures.size(); i++)
		{
			if (auto& t = resources->textures[i]; t.texture)
			{
				//UpdateBindlessTexture(t);

				auto& param = resources->resourceAutoParams[t.autoParamIdx];
				*(UINT*)&data.rootParams[param.bufferOffset] = t.texture->srvHeapIndex;
			}
		}
	}
}

void MaterialInstance::UpdatePerObject(MaterialDataStorage& data, const ShaderConstantsProvider& info)
{
	for (auto p : resources->objectAutoParams)
	{
		if (p.type == ResourcesInfo::AutoParam::WORLD_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getWorldMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::INV_WORLD_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(XMMatrixInverse(nullptr, info.getWorldMatrix())));
		else if (p.type == ResourcesInfo::AutoParam::PREV_WORLD_MATRIX)
			XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&data.rootParams[p.bufferOffset], XMMatrixTranspose(info.getPreviousWorldMatrix()));
		else if (p.type == ResourcesInfo::AutoParam::WORLD_POSITION)
			*(DirectX::XMFLOAT3*)&data.rootParams[p.bufferOffset] = info.getWorldPosition();
	}
}

void MaterialInstance::UpdateBindlessTexture(const ResourcesInfo::Texture& texture)
{
	auto& param = resources->resourceAutoParams[texture.autoParamIdx];

	auto& buff = resources->rootBuffer.defaultData;
	*(UINT*)&buff[param.bufferOffset] = texture.texture->srvHeapIndex;
}

void MaterialInstance::BindTextures(ID3D12GraphicsCommandList* commandList)
{
	for (UINT i = 0; i < resources->boundTexturesCount; i++)
	{
		auto& t = resources->textures[i];
		commandList->SetGraphicsRootDescriptorTable(t.rootIndex, t.texture->srvHandles);
	}

	for (auto& uav : resources->uavs)
		commandList->SetGraphicsRootUnorderedAccessView(uav.rootIndex, uav.uav->GetGPUVirtualAddress());
}

void MaterialInstance::BindConstants(ID3D12GraphicsCommandList* commandList, const MaterialDataStorage& data, const ShaderConstantsProvider& constants)
{
	if (data.rootParams.size())
		commandList->SetGraphicsRoot32BitConstants(resources->rootBuffer.rootIndex, data.rootParams.size(), data.rootParams.data(), 0);

	for (auto& b : resources->buffers)
	{
		if (b.type == GpuBufferType::Instancing || b.type == GpuBufferType::Geometry)
			commandList->SetGraphicsRootShaderResourceView(b.rootIndex, constants.getGeometryBuffer());
		else if (b.type == GpuBufferType::Global)
 			commandList->SetGraphicsRootConstantBufferView(b.rootIndex, b.globalCBuffer.data[constants.params.frameIndex]->GpuAddress());
	}
}

void AssignedMaterial::BindPipeline(ID3D12GraphicsCommandList* commandList)
{
	commandList->SetPipelineState(pipelineState);
}
