#pragma once

#include "AaRenderSystem.h"
#include "AaShaderBuffersManager.h"
#include "AaShaderFileParser.h"

enum ShaderType
{
	ShaderTypeVertex,
	ShaderTypeHull,
	ShaderTypeDomain,
	ShaderTypeGeometry,
	ShaderTypePixel,
};

struct CbufferInfo
{
	std::string name;
	uint32_t slot;
	uint32_t size = 0;

	struct ConstantInfo
	{
		std::string name;
		uint32_t position;
		uint32_t size;
	};
	std::vector<ConstantInfo> params;
};

struct CompiledShader
{
	std::vector<CbufferInfo> privateCbuffers;
	std::map<std::string, uint32_t> globalCbuffers;

	void* shader{};
	ID3DBlob* vsBuffer{};
};

struct LoadedShader
{
	shaderRef ref;
	CompiledShader* compiled = nullptr;
};

class AaShaderManager
{
public:

	AaShaderManager(AaRenderSystem* mRS);
	~AaShaderManager();

	static AaShaderManager& get();

	AaShaderBuffersManager buffers;

	void loadShaderReferences(std::string directory, bool subDirectories = false);

	LoadedShader* getVertexShader(const std::string& name);
	LoadedShader* getPixelShader(const std::string& name);

private:

	bool compileShaderRef(LoadedShader* shader, ShaderType);

	std::map<std::string, LoadedShader*> loadedShaders[5];

	AaRenderSystem* mRenderSystem;
};
