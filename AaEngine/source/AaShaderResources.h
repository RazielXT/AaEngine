#pragma once

#include "AaRenderSystem.h"
#include "AaShaderFileParser.h"
#include "AaShaderCompiler.h"
#include <dxcapi.h>
#include "Directx.h"

struct LoadedShader
{
	ShaderRef ref;
	ComPtr<IDxcBlob> blob;
	ShaderDescription desc;
	time_t filetime;
};

class AaShaderResources
{
public:

	AaShaderResources(AaRenderSystem* mRS);
	~AaShaderResources();

	static AaShaderResources& get();

	void loadShaderReferences(std::string directory, bool subDirectories = false);

	LoadedShader* getShader(const std::string& name, ShaderType);

	std::vector<LoadedShader*> reloadShaders();

private:

 	std::map<std::string, std::unique_ptr<LoadedShader>> loadedShaders[ShaderType_COUNT];

	AaRenderSystem* mRenderSystem;

	AaShaderCompiler compiler;
};
