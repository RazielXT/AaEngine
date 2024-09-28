#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "RenderTargetTexture.h"
#include "ResourcesManager.h"
#include "AaModel.h"
#include <dxcapi.h>
#include <memory>
#include "AaMaterialFileParser.h"
#include "AaShaderResources.h"
#include "MaterialInfo.h"
#include <map>
#include "AaMaterialConstants.h"

using namespace Microsoft::WRL;
using namespace DirectX;

class AaCamera;
class AaModel;
class AaEntity;
class MaterialInstance;
class AaMaterial;

class MaterialBase
{
public:

	MaterialBase(AaRenderSystem* renderSystem, ResourcesManager& mgr, const MaterialRef& ref);
	~MaterialBase();

	void Load();

	void BindSignature(ID3D12GraphicsCommandList* commandList, int frameIndex) const;

	AaMaterial* GetAssignedMaterial(MaterialInstance* instance, const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout);
	void ReloadPipeline();
	bool ContainsShader(const LoadedShader*) const;

	std::unique_ptr<MaterialInstance> CreateMaterialInstance(const MaterialRef& childRef);

	ResourcesManager& mgr;

	SignatureInfo info;

private:

	std::shared_ptr<ResourcesInfo> CreateResourcesData(const MaterialRef& childRef) const;

	void CreateRootSignature();

	LoadedShader* shaders[ShaderType_COUNT]{};

	ID3D12RootSignature* rootSignature;
	AaRenderSystem* renderSystem;

	ID3D12PipelineState* GetPipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout);
	ID3D12PipelineState* CreatePipelineState(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout);
	struct PipelineStateData
	{
		size_t hashId;
		std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
		ID3D12PipelineState* pipeline;
	};
	std::vector<PipelineStateData> pipelineStates;

	std::map<MaterialInstance*, std::vector<std::unique_ptr<AaMaterial>>> assignedMaterials;

	const MaterialRef& ref;

	bool loaded = false;
};

class MaterialInstance
{
	friend MaterialBase;
public:

	MaterialInstance(MaterialBase&, const MaterialRef& ref);

	MaterialInstance(const MaterialInstance& other) : base(other.base), ref(other.ref)
	{
		resources = other.resources;
	}
	~MaterialInstance();

	void AddTexture(ShaderTextureView& texture);
	void SetTexture(ShaderTextureView& texture, UINT slot);
	ShaderTextureView* GetTexture(UINT slot) const;

	void SetParameter(const std::string& name, float* value, size_t size);

	void LoadMaterialConstants(MaterialConstantBuffers& buffers) const;
	void UpdatePerFrame(MaterialConstantBuffers& buffers, const FrameGpuParameters& info);
	void UpdatePerObject(MaterialConstantBuffers& buffers, const XMFLOAT4X4& wvpMatrix, const XMMATRIX& worldMatrix, const FrameGpuParameters& info);

	AaMaterial* Assign(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout);

	const MaterialBase* GetBase() const;

protected:

	MaterialBase& base;
	const MaterialRef& ref;

	std::shared_ptr<ResourcesInfo> resources;

	void UpdateMaterial();
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
		return pipelineState = pipeline;
	}

	void BindTextures(ID3D12GraphicsCommandList* commandList, int frameIndex);
	void BindConstants(ID3D12GraphicsCommandList* commandList, int frameIndex, const MaterialConstantBuffers& buffers);
	void BindPipeline(ID3D12GraphicsCommandList* commandList);

private:

	ID3D12PipelineState* pipelineState = nullptr;
};