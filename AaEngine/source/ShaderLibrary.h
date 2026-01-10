#pragma once

#include "RenderSystem.h"
#include "ShaderFileParser.h"
#include "ShaderCompiler.h"
#include <dxcapi.h>
#include "Directx.h"

struct LoadedShader
{
	ShaderRef ref;
	ComPtr<IDxcBlob> blob;
	ShaderDescription desc;
	time_t filetime;
};

class ShaderLibrary
{
public:

	ShaderLibrary(const ShaderDefines&);
	~ShaderLibrary();

	void loadShaderReferences(std::string directory, bool subDirectories = false);
	void addShaderReferences(const shaderRefMaps& maps);

	const LoadedShader* getShader(const std::string& name, ShaderType);
	const LoadedShader* getShader(const std::string& name, ShaderType, const ShaderRef& ref);

	std::vector<const LoadedShader*> reloadShadersChangedFiles();
	std::vector<const LoadedShader*> reloadShadersWithDefine(const std::string&);

	std::set<std::string> getKnownDefines() const;

private:

	ShaderTypesArray<std::map<std::string, std::unique_ptr<LoadedShader>>> loadedShaders;

	ShaderCompiler compiler;

	const ShaderDefines& defines;
};
