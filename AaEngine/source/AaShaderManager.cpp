#include "AaShaderManager.h"
#include "AaLogger.h"
#include "AaEntity.h"
#include "Directories.h"
#include "AaShaderFileParser.h"

AaShaderManager* instance = nullptr;

AaShaderManager::AaShaderManager(AaRenderSystem* mRS) : buffers(mRS)
{
	if (instance)
		throw std::exception("duplicate AaShaderManager");

	mRenderSystem = mRS;
	instance = this;
}

AaShaderManager::~AaShaderManager()
{
	for (auto& it : loadedShaders[ShaderTypeVertex])
	{
		if (it.second->compiled)
		{
			((ID3D11VertexShader*)it.second->compiled->shader)->Release();
			it.second->compiled->vsBuffer->Release();
			delete it.second->compiled;
		}
	}

	for (auto& it : loadedShaders[ShaderTypePixel])
	{
		if (it.second->compiled)
		{
			((ID3D11PixelShader*)it.second->compiled->shader)->Release();
			delete it.second->compiled;
		}
	}
}

AaShaderManager& AaShaderManager::get()
{
	return *instance;
}

void AaShaderManager::loadShaderReferences(std::string directory, bool subDirectories)
{
	shaderRefMaps maps = AaShaderFileParser::parseAllShaderFiles(directory, subDirectories);

	for (auto& r : maps.vertexShaderRefs)
	{
		auto& info = loadedShaders[ShaderTypeVertex][r.first];
		if (info == nullptr)
			info = new LoadedShader{ r.second };
		else
			AaLogger::logWarning("duplicate vertex shader ref " + r.first);
	}
	for (auto& r : maps.pixelShaderRefs)
	{
		auto& info = loadedShaders[ShaderTypePixel][r.first];
		if (info == nullptr)
			info = new LoadedShader{ r.second };
		else
			AaLogger::logWarning("duplicate pixel shader ref " + r.first);
	}
}

LoadedShader* AaShaderManager::getVertexShader(const std::string& name)
{
	auto it = loadedShaders[ShaderTypeVertex].find(name);

	if (it == loadedShaders[ShaderTypeVertex].end())
		return nullptr;

	if (!it->second->compiled)
		compileShaderRef(it->second, ShaderTypeVertex);

	return it->second;
}

LoadedShader* AaShaderManager::getPixelShader(const std::string& name)
{
	auto it = loadedShaders[ShaderTypePixel].find(name);

	if (it == loadedShaders[ShaderTypePixel].end())
		return nullptr;

	if (!it->second->compiled)
		compileShaderRef(it->second, ShaderTypePixel);

	return it->second;
}

static std::vector<D3D_SHADER_MACRO> MakeShaderDefines(const shaderRef& ref)
{
	std::vector<D3D_SHADER_MACRO> defines;
	if (!ref.defines.empty())
	{
		for (auto& d : ref.defines)
		{
			defines.push_back(D3D_SHADER_MACRO{ d.first.data(), d.second.data() });
		}
		defines.push_back(D3D_SHADER_MACRO{ nullptr, nullptr });
	}
	return defines;
}

static void ReadConstantBufferLayout(LoadedShader& shader, ID3DBlob* pShaderBytecode)
{
	ID3D11ShaderReflection* pReflector = nullptr;
	D3DReflect(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&pReflector);

	D3D11_SHADER_DESC shaderDesc;
	pReflector->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
	{
		ID3D11ShaderReflectionConstantBuffer* pConstantBuffer = pReflector->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC bufferDesc;
		pConstantBuffer->GetDesc(&bufferDesc);

		// Get the register assignment
		D3D11_SHADER_INPUT_BIND_DESC bindDesc;
		pReflector->GetResourceBindingDescByName(bufferDesc.Name, &bindDesc);

		auto it = shader.ref.privateCbuffers.find(bufferDesc.Name);
		if (it != shader.ref.privateCbuffers.end())
		{
			CbufferInfo bufferInfo;
			bufferInfo.name = bufferDesc.Name;
			bufferInfo.slot = bindDesc.BindPoint;

			for (UINT j = 0; j < bufferDesc.Variables; ++j)
			{
				ID3D11ShaderReflectionVariable* pVariable = pConstantBuffer->GetVariableByIndex(j);
				D3D11_SHADER_VARIABLE_DESC varDesc;
				pVariable->GetDesc(&varDesc);

				CbufferInfo::ConstantInfo info;
				info.name = varDesc.Name;
				info.position = varDesc.StartOffset;
				info.size = varDesc.Size;
				bufferInfo.params.push_back(info);

				bufferInfo.size = max(bufferInfo.size, varDesc.StartOffset + varDesc.Size);
			}

			shader.compiled->privateCbuffers.push_back(bufferInfo);
		}
		else
		{
			shader.compiled->globalCbuffers[bufferDesc.Name] = bindDesc.BindPoint;
		}
	}

	pReflector->Release();
}

bool AaShaderManager::compileShaderRef(LoadedShader* shader, ShaderType type)
{
	UINT shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	shaderFlags |= D3DCOMPILE_DEBUG;
#endif

	std::wstring filepath(SHADER_HLSL_DIRECTORY.begin(), SHADER_HLSL_DIRECTORY.end());
	filepath.insert(filepath.end(), shader->ref.file.begin(), shader->ref.file.end());

	auto defines = MakeShaderDefines(shader->ref);

	ID3DBlob* shBuffer = nullptr;
	ID3DBlob* errorBuffer = nullptr;
	auto result = D3DCompileFromFile(filepath.c_str(), defines.empty() ? nullptr : defines.data(), nullptr, shader->ref.entry.c_str(), shader->ref.profile.c_str(), shaderFlags, 0, &shBuffer, &errorBuffer);

	if (errorBuffer)
	{
		AaLogger::logWarning((char*)errorBuffer->GetBufferPointer());
		errorBuffer->Release();
	}

	if (FAILED(result))
	{
		AaLogger::logErrorD3D(shader->ref.file, result);
		return false;
	}

	void* shaderInstance = nullptr;

	if (type == ShaderTypeVertex)
	{
		result = mRenderSystem->getDevice()->CreateVertexShader(shBuffer->GetBufferPointer(), shBuffer->GetBufferSize(), nullptr, (ID3D11VertexShader**)&shaderInstance);
	}
	else if (type == ShaderTypePixel)
	{
		result = mRenderSystem->getDevice()->CreatePixelShader(shBuffer->GetBufferPointer(), shBuffer->GetBufferSize(), nullptr, (ID3D11PixelShader**)&shaderInstance);
	}

	if (FAILED(result))
	{
		AaLogger::logErrorD3D(shader->ref.file + " " + shader->ref.entry, result);
		shBuffer->Release();
		return false;
	}

	shader->compiled = new CompiledShader();
	shader->compiled->shader = shaderInstance;

	if (type == ShaderTypeVertex)
		shader->compiled->vsBuffer = shBuffer;

	ReadConstantBufferLayout(*shader, shBuffer);

	return true;
}
