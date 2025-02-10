#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "RenderTargetTexture.h"
#include "VertexBufferModel.h"
#include <dxcapi.h>
#include <memory>
#include "MaterialFileParser.h"
#include "ShaderLibrary.h"
#include "ShaderResources.h"
#include "ShaderSignature.h"
#include <map>
#include <array>

using namespace Microsoft::WRL;
using namespace DirectX;

class Camera;
class VertexBufferModel;
class SceneEntity;
class MaterialInstance;
class AssignedMaterial;
struct GraphicsResources;

struct MaterialDataStorage
{
	std::vector<float> rootParams;
};

class MaterialBase
{
public:

	MaterialBase(ID3D12Device& device, const MaterialRef& ref);
	~MaterialBase();

	void Load(ShaderLibrary& shaderLib);

	void BindSignature(ID3D12GraphicsCommandList* commandList) const;

	AssignedMaterial* GetAssignedMaterial(MaterialInstance* instance, const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, MaterialTechnique technique);
	void ReloadPipeline(ShaderLibrary& shaderLib);
	bool ContainsShader(const LoadedShader*) const;

	std::unique_ptr<MaterialInstance> CreateMaterialInstance(const MaterialRef& childRef, GraphicsResources& r, ResourceUploadBatch& batch);

	const char* GetTechniqueOverride(MaterialTechnique technique) const;

	SignatureInfo info;

private:

	void CreateResourcesData(MaterialInstance& instance, GraphicsResources& r, ResourceUploadBatch& batch) const;

	const LoadedShader* shaders[ShaderType_COUNT]{};

	ID3D12RootSignature* rootSignature{};
	ID3D12Device& device;

	struct TechniqueProperties
	{
		D3D12_COMPARISON_FUNC comparisonFunc;
		float slopeScaledDepthBias;
	};

	ID3D12PipelineState* GetPipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, MaterialTechnique);
	ID3D12PipelineState* CreatePipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, const TechniqueProperties&);
	struct PipelineStateData
	{
		size_t hashId;
		std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
		std::vector<DXGI_FORMAT> target;
		TechniqueProperties properties;
		ID3D12PipelineState* pipeline;
	};
	std::vector<PipelineStateData> pipelineStates;

	std::map<MaterialInstance*, std::vector<std::unique_ptr<AssignedMaterial>>> assignedMaterials;

	const MaterialRef& ref;
};

struct MaterialPropertiesOverrideDescription
{
	struct Input
	{
		std::string name;
		UINT sizeBytes;
		float value[4];
	};
	std::vector<Input> params;
};

struct MaterialPropertiesOverride
{
	struct Input
	{
		UINT offsetFloats;
		UINT sizeBytes;
		float value[4];
	};
	std::vector<Input> params;
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

	void SetUAV(ID3D12Resource* uav, UINT slot);

	std::unique_ptr<MaterialPropertiesOverride> CreateParameterOverride(const MaterialPropertiesOverrideDescription& description) const;
	void ApplyParametersOverride(MaterialPropertiesOverride& data, MaterialDataStorage& output) const;

	void SetParameter(const std::string& name, const void* value, size_t count);
	void SetParameter(const std::string& name, float* buffer, const void* value, size_t count) const;
	void GetParameter(const std::string& name, float* output) const;

	void SetParameter(ResourcesInfo::AutoParam, const void* value, size_t count);
	void SetParameter(ResourcesInfo::AutoParam, const void* value, size_t count, MaterialDataStorage& data);

	void SetParameter(FastParam param, const void* value);
	void SetParameter(FastParam param, const void* value, MaterialDataStorage& data);
	void GetParameter(FastParam param, float* output) const;
	UINT GetParameterOffset(FastParam id) const;

	void LoadMaterialConstants(MaterialDataStorage& buffers) const;
	void UpdatePerFrame(MaterialDataStorage& data, const ShaderConstantsProvider& info);
	void UpdatePerObject(MaterialDataStorage& buffers, const ShaderConstantsProvider& info);

	void BindTextures(ID3D12GraphicsCommandList* commandList);
	void BindConstants(ID3D12GraphicsCommandList* commandList, const MaterialDataStorage& data, const ShaderConstantsProvider& buffers);

	AssignedMaterial* Assign(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout, const std::vector<DXGI_FORMAT>& target, MaterialTechnique technique =  MaterialTechnique::Default);

	const MaterialBase* GetBase() const;

	const char* GetTechniqueOverride(MaterialTechnique technique) const;

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

	void UpdateBindlessTexture(const ResourcesInfo::Texture& texture);
};

class AssignedMaterial : public MaterialInstance
{
	friend MaterialBase;
public:

	AssignedMaterial(const MaterialInstance& other, ID3D12PipelineState* pipeline) : MaterialInstance(other), pipelineState(pipeline)
	{
	}

	AssignedMaterial(const AssignedMaterial& other) : MaterialInstance(other)
	{
		pipelineState = other.pipelineState;
	}

	bool operator==(ID3D12PipelineState* pipeline)
	{
		return pipelineState == pipeline;
	}

	void BindPipeline(ID3D12GraphicsCommandList* commandList);

	MaterialInstance* origin{};

private:

	ID3D12PipelineState* pipelineState = nullptr;
};