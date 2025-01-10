#include "ShaderCompiler.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <ranges>
#include "directx\d3dx12.h"
#include "Directories.h"
#include "FileLogger.h"
#include "VertexBufferModel.h"
#include <unordered_set>
#include "StringUtils.h"

ShaderCompiler::ShaderCompiler()
{
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
}

ShaderCompiler::~ShaderCompiler()
{

}

static DXGI_FORMAT GetFormatFromSignatureParameterDesc(const D3D12_SIGNATURE_PARAMETER_DESC& desc)
{
	if (desc.SemanticName == std::string_view("COLOR"))
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	// Determine the number of components based on the Mask
	int componentCount = 0;
	for (UINT mask = desc.Mask; mask; mask >>= 1) {
		if (mask & 1) {
			componentCount++;
		}
	}

	// Determine the format based on ComponentType and componentCount
	switch (desc.ComponentType) {
	case D3D_REGISTER_COMPONENT_UINT32:
		switch (componentCount) {
		case 1: return DXGI_FORMAT_R32_UINT;
		case 2: return DXGI_FORMAT_R32G32_UINT;
		case 3: return DXGI_FORMAT_R32G32B32_UINT;
		case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
		}
		break;
	case D3D_REGISTER_COMPONENT_SINT32:
		switch (componentCount) {
		case 1: return DXGI_FORMAT_R32_SINT;
		case 2: return DXGI_FORMAT_R32G32_SINT;
		case 3: return DXGI_FORMAT_R32G32B32_SINT;
		case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
		}
		break;
	case D3D_REGISTER_COMPONENT_FLOAT32:
		switch (componentCount) {
		case 1: return DXGI_FORMAT_R32_FLOAT;
		case 2: return DXGI_FORMAT_R32G32_FLOAT;
		case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
		break;
	}

	// Default case if no match is found
	return DXGI_FORMAT_UNKNOWN;
}

bool ShaderCompiler::reflectShaderInfo(IDxcResult* compiledShaderBuffer, ShaderDescription& description, ShaderType type)
{
	// Get shader reflection data.
	ComPtr<IDxcBlob> reflectionBlob{};
	compiledShaderBuffer->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr);

	if (!reflectionBlob)
		return false;

	const DxcBuffer reflectionBuffer
	{
		.Ptr = reflectionBlob->GetBufferPointer(),
		.Size = reflectionBlob->GetBufferSize(),
		.Encoding = 0,
	};

	ComPtr<ID3D12ShaderReflection> shaderReflection{};
	pUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
	D3D12_SHADER_DESC shaderDesc{};
	shaderReflection->GetDesc(&shaderDesc);

	if (type == ShaderTypeVertex)
	{
		description.inputLayout.reserve(shaderDesc.InputParameters);

		for (const uint32_t parameterIndex : std::views::iota(0u, shaderDesc.InputParameters))
		{
			D3D12_SIGNATURE_PARAMETER_DESC signatureParameterDesc{};
			shaderReflection->GetInputParameterDesc(parameterIndex, &signatureParameterDesc);

			description.inputLayout.emplace_back(D3D12_INPUT_ELEMENT_DESC{
						.SemanticName = VertexElementSemantic::GetConstName(signatureParameterDesc.SemanticName),
						.SemanticIndex = signatureParameterDesc.SemanticIndex,
						.Format = GetFormatFromSignatureParameterDesc(signatureParameterDesc),
						.InputSlot = 0u,
						.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
						.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
						// There doesn't seem to be a obvious way to 
						// automate this currently, which might be a issue when instanced rendering is used
						.InstanceDataStepRate = 0u,
				});
		}
	}
	if (type == ShaderTypePixel)
	{
		description.outputTargets = shaderDesc.OutputParameters;
	}

	for (const uint32_t i : std::views::iota(0u, shaderDesc.BoundResources))
	{
		D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
		shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc);

		if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER)
		{
			auto shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

			ShaderReflection::CBuffer cbufferInfo;
			cbufferInfo.Name = constantBufferDesc.Name;
			cbufferInfo.Size = 0;// constantBufferDesc.Size;
			cbufferInfo.Slot = shaderInputBindDesc.BindPoint;
			cbufferInfo.Space = shaderInputBindDesc.Space;

			for (const uint32_t i : std::views::iota(0u, constantBufferDesc.Variables))
			{
				auto v = shaderReflectionConstantBuffer->GetVariableByIndex(i);
				D3D12_SHADER_VARIABLE_DESC desc;
				v->GetDesc(&desc);

				cbufferInfo.Size = max(cbufferInfo.Size, desc.StartOffset + desc.Size);

				cbufferInfo.Params.emplace_back(ShaderReflection::CBuffer::Parameter{
					.Name = desc.Name,
					.StartOffset = desc.StartOffset,
					.Size = desc.Size,
					});
			}

			description.cbuffers.push_back(cbufferInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_TEXTURE)
		{
			ShaderReflection::Texture texInfo;
			texInfo.Name = shaderInputBindDesc.Name;
			texInfo.Slot = shaderInputBindDesc.BindPoint;
			texInfo.Space = shaderInputBindDesc.Space;

			description.textures.push_back(texInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_SAMPLER)
		{
			ShaderReflection::Sampler sInfo;
			sInfo.Name = shaderInputBindDesc.Name;
			sInfo.Slot = shaderInputBindDesc.BindPoint;

			description.samplers.push_back(sInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_UAV_RWTYPED)
		{
			ShaderReflection::UAV uavInfo;
			uavInfo.Name = shaderInputBindDesc.Name;
			uavInfo.Slot = shaderInputBindDesc.BindPoint;
			uavInfo.Space = shaderInputBindDesc.Space;

			description.uavs.push_back(uavInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_STRUCTURED)
		{
			ShaderReflection::StructuredBuffer uavInfo;
			uavInfo.Name = shaderInputBindDesc.Name;
			uavInfo.Slot = shaderInputBindDesc.BindPoint;
			uavInfo.Space = shaderInputBindDesc.Space;

			description.structuredBuffers.push_back(uavInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED || shaderInputBindDesc.Type == D3D_SIT_UAV_RWBYTEADDRESS)
		{
			ShaderReflection::StructuredBuffer uavInfo;
			uavInfo.Name = shaderInputBindDesc.Name;
			uavInfo.Slot = shaderInputBindDesc.BindPoint;
			uavInfo.Space = shaderInputBindDesc.Space;

			description.rwStructuredBuffers.push_back(uavInfo);
		}
	}

	if (description.textures.empty() && shaderDesc.TextureNormalInstructions)
	{
		description.bindlessTextures = true;
	}

	return true;
}

class CustomIncludeHandler : public IDxcIncludeHandler
{
public:

	IDxcUtils* pUtils;

	HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
	{
		ComPtr<IDxcBlobEncoding> pEncoding;

		if (IncludedFiles.find(pFilename) != IncludedFiles.end())
		{
			// Return empty string blob if this file has been included before
			static const char nullStr[] = " ";
			pUtils->CreateBlobFromPinned(nullStr, ARRAYSIZE(nullStr), DXC_CP_ACP, pEncoding.GetAddressOf());
			*ppIncludeSource = pEncoding.Detach();
			return S_OK;
		}

		auto fullpath = as_wstring(SHADER_HLSL_DIRECTORY) + pFilename;
		HRESULT hr = pUtils->LoadFile(fullpath.c_str(), nullptr, pEncoding.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			IncludedFiles.insert(pFilename);
			*ppIncludeSource = pEncoding.Detach();
		}
		return hr;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override { return E_NOINTERFACE; }
	ULONG STDMETHODCALLTYPE AddRef(void) override { return 0; }
	ULONG STDMETHODCALLTYPE Release(void) override { return 0; }

	std::unordered_set<std::wstring> IncludedFiles;
};

static std::vector<std::wstring> MakeDefines(const ShaderRef& ref)
{
	std::vector<std::wstring> out;

	for (auto& d : ref.defines)
	{
		out.emplace_back(d.first.begin(), d.first.end());
	}

	return out;
}

ComPtr<IDxcBlob> ShaderCompiler::compileShader(const ShaderRef& ref, ShaderDescription& description, ShaderType type)
{
	auto path = SHADER_HLSL_DIRECTORY + ref.file;
 	auto wfile = as_wstring(path);

	// Load shader source code from file
	ComPtr<IDxcBlobEncoding> pSourceBlob;
	pUtils->LoadFile(wfile.c_str(), nullptr, &pSourceBlob);

	if (!pSourceBlob)
	{
		FileLogger::logError("failed to load file " + path);
		return nullptr;
	}

	auto wentry = as_wstring(ref.entry);
 	auto wprofile = as_wstring(ref.profile);

	// Set up compile arguments
	std::vector<LPCWSTR> arguments = { L"-E", wentry.c_str() , L"-T", wprofile.c_str() };
#ifdef _DEBUG
	arguments.insert(arguments.end(), { L"-Zi", L"-Qembed_debug"});
	//arguments.push_back(L"-Od");
#else
	arguments.push_back(L"-Qstrip_debug");
	arguments.push_back(L"-Qstrip_reflect");
#endif // _DEBUG

	auto defines = MakeDefines(ref);
	for (auto& d : defines)
		arguments.insert(arguments.end(), { L"-D", d.c_str() });

	DxcBuffer sourceBuffer;
	sourceBuffer.Ptr = pSourceBlob->GetBufferPointer();
	sourceBuffer.Size = pSourceBlob->GetBufferSize();
	sourceBuffer.Encoding = 0;

	CustomIncludeHandler includeHandler;
	includeHandler.pUtils = pUtils.Get();

	ComPtr<IDxcResult> pCompileResult;
	auto hr = pCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), &includeHandler, IID_PPV_ARGS(pCompileResult.GetAddressOf()));
	if (FAILED(hr))
	{
		FileLogger::logErrorD3D("compileShader " + ref.entry, hr);
		return nullptr;
	}

	for (auto& h : includeHandler.IncludedFiles)
	{
		description.includes.push_back(as_string(h));
	}

	// Check for compilation errors
	ComPtr<IDxcBlobUtf8> pErrors;
	pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	if (pErrors && pErrors->GetStringLength() > 0)
	{
		OutputDebugStringA(pErrors->GetStringPointer());
		FileLogger::logWarning(pErrors->GetStringPointer());

		return nullptr;
	}

	if (!reflectShaderInfo(pCompileResult.Get(), description, type))
	{
		FileLogger::logError("compileShader no reflection " + ref.entry);
		return nullptr;
	}

	ComPtr<IDxcBlob> pShaderBlob;
	hr = pCompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShaderBlob), nullptr);

	return pShaderBlob;
}
