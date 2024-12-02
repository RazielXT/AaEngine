#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "RenderTargetTexture.h"
#include "DescriptorManager.h"
#include "AaModel.h"
#include <dxcapi.h>
#include <memory>
#include "AaMaterialFileParser.h"
#include "AaShaderLibrary.h"
#include "ShaderResources.h"
#include "ShaderSignature.h"
#include <map>
#include <array>

using namespace Microsoft::WRL;
using namespace DirectX;

class AaCamera;
class AaModel;
class AaEntity;
class MaterialInstance;
class AaMaterial;

struct MaterialDataStorage
{
	std::vector<float> rootParams;
};

class MaterialBase
{
public:

	MaterialBase(RenderSystem* renderSystem, DescriptorManager& mgr, const MaterialRef& ref);
	~MaterialBase();

	void Load();

	void BindSignature(ID3D12GraphicsCommandList* commandList) const;

	AaMaterial* GetAssignedMaterial(MaterialInstance* instance, const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target);
	void ReloadPipeline();
	bool ContainsShader(const LoadedShader*) const;

	std::unique_ptr<MaterialInstance> CreateMaterialInstance(const MaterialRef& childRef, ResourceUploadBatch& batch);

	const char* GetTechniqueOverride(MaterialTechnique technique) const;

	DescriptorManager& mgr;

	SignatureInfo info;

private:

	void CreateResourcesData(MaterialInstance& instance, ResourceUploadBatch& batch) const;

	LoadedShader* shaders[ShaderType_COUNT]{};

	ID3D12RootSignature* rootSignature{};
	RenderSystem* renderSystem;

	ID3D12PipelineState* GetPipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target);
	ID3D12PipelineState* CreatePipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target);
	struct PipelineStateData
	{
		size_t hashId;
		std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
		std::vector<DXGI_FORMAT> target;
		ID3D12PipelineState* pipeline;
	};
	std::vector<PipelineStateData> pipelineStates;

	std::map<MaterialInstance*, std::vector<std::unique_ptr<AaMaterial>>> assignedMaterials;

	const MaterialRef& ref;
};

class MaterialInstance
{
	friend MaterialBase;
public:

	MaterialInstance(MaterialBase&, const MaterialRef& ref);

	MaterialInstance(const MaterialInstance& other) : base(other.base), ref(other.ref)
	{
		resources = other.resources;
		paramsTable = other.paramsTable;
	}
	~MaterialInstance();

	void SetTexture(ShaderTextureView& texture, UINT slot);
	ShaderTextureView* GetTexture(UINT slot) const;

	void SetParameter(const std::string& name, float* value, size_t size);
	void GetParameter(const std::string& name, float* output) const;

	void SetParameter(FastParam param, float* value);
	void GetParameter(FastParam param, float* output) const;
	UINT GetParameterOffset(FastParam param) const;

	void LoadMaterialConstants(MaterialDataStorage& buffers) const;
	void UpdatePerFrame(MaterialDataStorage& data, const ShaderConstantsProvider& info);
	void UpdatePerObject(MaterialDataStorage& buffers, const ShaderConstantsProvider& info);

	void BindTextures(ID3D12GraphicsCommandList* commandList);
	void BindConstants(ID3D12GraphicsCommandList* commandList, const MaterialDataStorage& data, const ShaderConstantsProvider& buffers);

	AaMaterial* Assign(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target);

	const MaterialBase* GetBase() const;

	bool HasInstancing() const;
	bool IsTransparent() const;

protected:

	MaterialBase& base;
	const MaterialRef& ref;

	std::shared_ptr<ResourcesInfo> resources;

	struct FastParamInfo
	{
		float* data{};
		UINT Size{};
		UINT Offset{};
	};
	std::array<FastParamInfo, (int)FastParam::COUNT> paramsTable{};

	void SetFastTableParameter(const std::string& name, float* data, UINT size, UINT offset);

	void UpdateBindlessTexture(const ShaderTextureView& texture, UINT slot);
};

class AaMaterial : public MaterialInstance
{
	friend MaterialBase;
public:

	AaMaterial(const MaterialInstance& other, ID3D12PipelineState* pipeline) : MaterialInstance(other), pipelineState(pipeline)
	{
	}

	AaMaterial(const AaMaterial& other) : MaterialInstance(other)
	{
		pipelineState = other.pipelineState;
	}

	bool operator==(ID3D12PipelineState* pipeline)
	{
		return pipelineState == pipeline;
	}

	void BindPipeline(ID3D12GraphicsCommandList* commandList);

private:

	ID3D12PipelineState* pipelineState = nullptr;
};