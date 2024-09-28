#include "AaShaderCompiler.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <ranges>
#include "directx\d3dx12.h"
#include "Directories.h"
#include "AaLogger.h"
#include "AaModel.h"

AaShaderCompiler::AaShaderCompiler(AaRenderSystem * mRS)
{
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
}

AaShaderCompiler::~AaShaderCompiler()
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

bool AaShaderCompiler::reflectShaderInfo(IDxcResult* compiledShaderBuffer, ShaderDescription& description)
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

	// Setup the input assembler. Only applicable for vertex shaders.
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

	for (const uint32_t i : std::views::iota(0u, shaderDesc.BoundResources))
	{
		D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc{};
		shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc);

		if (shaderInputBindDesc.Type == D3D_SIT_CBUFFER)
		{
			auto shaderReflectionConstantBuffer = shaderReflection->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
			shaderReflectionConstantBuffer->GetDesc(&constantBufferDesc);

			CBufferInfo cbufferInfo;
			cbufferInfo.Name = constantBufferDesc.Name;
			cbufferInfo.Size = constantBufferDesc.Size;
			cbufferInfo.Slot = shaderInputBindDesc.BindPoint;
			cbufferInfo.Space = shaderInputBindDesc.Space;

			for (const uint32_t i : std::views::iota(0u, constantBufferDesc.Variables))
			{
				auto v = shaderReflectionConstantBuffer->GetVariableByIndex(i);
				D3D12_SHADER_VARIABLE_DESC desc;
				v->GetDesc(&desc);
				cbufferInfo.Params.emplace_back(CBufferInfo::Parameter{
					.Name = desc.Name,
					.StartOffset = desc.StartOffset,
					.Size = desc.Size,
					});
			}

			description.cbuffers.push_back(cbufferInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_TEXTURE)
		{
			TextureInfo texInfo;
			texInfo.Name = shaderInputBindDesc.Name;
			texInfo.Slot = shaderInputBindDesc.BindPoint;
			texInfo.Space = shaderInputBindDesc.Space;

			description.textures.push_back(texInfo);
		}
		else if (shaderInputBindDesc.Type == D3D_SIT_SAMPLER)
		{
			SamplerInfo sInfo;
			sInfo.Name = shaderInputBindDesc.Name;
			sInfo.Slot = shaderInputBindDesc.BindPoint;

			description.samplers.push_back(sInfo);
		}
	}

	if (description.textures.empty() && shaderDesc.TextureNormalInstructions)
	{
		description.bindlessTextures = true;
	}

	return true;
}

// {
// 	static ComPtr<IDxcUtils> pUtils;
// 	if (!pUtils)
// 		VERIFY_HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(pUtils.GetAddressOf())));
// 
// 	class CustomIncludeHandler : public IDxcIncludeHandler
// 	{
// 	public:
// 		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
// 		{
// 			ComPtr<IDxcBlobEncoding> pEncoding;
// 			std::string path = Paths::Normalize(UNICODE_TO_MULTIBYTE(pFilename));
// 			if (IncludedFiles.find(path) != IncludedFiles.end())
// 			{
// 				// Return empty string blob if this file has been included before
// 				static const char nullStr[] = " ";
// 				pUtils->CreateBlobFromPinned(nullStr, ARRAYSIZE(nullStr), DXC_CP_ACP, pEncoding.GetAddressOf());
// 				*ppIncludeSource = pEncoding.Detach();
// 				return S_OK;
// 			}
// 
// 			HRESULT hr = pUtils->LoadFile(pFilename, nullptr, pEncoding.GetAddressOf());
// 			if (SUCCEEDED(hr))
// 			{
// 				IncludedFiles.insert(path);
// 				*ppIncludeSource = pEncoding.Detach();
// 			}
// 			return hr;
// 		}
// 
// 		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override { return E_NOINTERFACE; }
// 		ULONG STDMETHODCALLTYPE AddRef(void) override { return 0; }
// 		ULONG STDMETHODCALLTYPE Release(void) override { return 0; }
// 
// 		std::unordered_set<std::string> IncludedFiles;
// 	};
// }
ComPtr<IDxcBlob> AaShaderCompiler::compileShader(const ShaderRef& ref, ShaderDescription& description)
{
	auto path = SHADER_HLSL_DIRECTORY + ref.file;
 	auto wfile = std::wstring(path.begin(), path.end());

	// Load shader source code from file
	ComPtr<IDxcBlobEncoding> pSourceBlob;
	pUtils->LoadFile(wfile.c_str(), nullptr, &pSourceBlob);

	auto wentry = std::wstring(ref.entry.begin(), ref.entry.end());
 	auto wprofile = std::wstring(ref.profile.begin(), ref.profile.end());

	// Set up compile arguments
	std::vector<LPCWSTR> arguments = { L"-E", wentry.c_str() , L"-T", wprofile.c_str() };
#ifdef _DEBUG
	arguments.insert(arguments.end(), { L"-Zi", L"-Qembed_debug" });
#else
	arguments.push_back(L"-Qstrip_debug");
	arguments.push_back(L"-Qstrip_reflect");
#endif // _DEBUG

	DxcBuffer sourceBuffer;
	sourceBuffer.Ptr = pSourceBlob->GetBufferPointer();
	sourceBuffer.Size = pSourceBlob->GetBufferSize();
	sourceBuffer.Encoding = 0;

	ComPtr<IDxcResult> pCompileResult;
	auto hr = pCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), nullptr, IID_PPV_ARGS(pCompileResult.GetAddressOf()));
	if (FAILED(hr))
	{
		AaLogger::logErrorD3D("compileShader " + ref.entry, hr);
		return nullptr;
	}

	// Check for compilation errors
	ComPtr<IDxcBlobUtf8> pErrors;
	pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	if (pErrors && pErrors->GetStringLength() > 0)
	{
		OutputDebugStringA(pErrors->GetStringPointer());
		AaLogger::logWarning(pErrors->GetStringPointer());

		return nullptr;
	}

	if (!reflectShaderInfo(pCompileResult.Get(), description))
	{
		AaLogger::logError("compileShader no reflection " + ref.entry);
		return nullptr;
	}

	ComPtr<IDxcBlob> pShaderBlob;
	hr = pCompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShaderBlob), nullptr);

	return pShaderBlob;
}
