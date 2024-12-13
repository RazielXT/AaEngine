#pragma once

#include "RenderSystem.h"
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

class AaShaderLibrary
{
public:

	AaShaderLibrary(RenderSystem* mRS);
	~AaShaderLibrary();

	static AaShaderLibrary& get();

	void loadShaderReferences(std::string directory, bool subDirectories = false);
	void addShaderReferences(const shaderRefMaps& maps);

	LoadedShader* getShader(const std::string& name, ShaderType);
	LoadedShader* getShader(const std::string& name, ShaderType, const ShaderRef& ref);

	std::vector<LoadedShader*> reloadShaders();

private:

 	std::map<std::string, std::unique_ptr<LoadedShader>> loadedShaders[ShaderType_COUNT];

	RenderSystem* mRenderSystem;

	AaShaderCompiler compiler;
};
