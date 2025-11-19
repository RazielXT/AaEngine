#include "ShaderDefines.h"
#include "GraphicsResources.h"

ShaderDefines::ShaderDefines(GraphicsResources& r) : resources(r)
{
}

void ShaderDefines::setDefine(const std::string& d)
{
	defines.insert(d);
	resources.materials.reloadShadersWithDefine(d);
}

void ShaderDefines::removeDefine(const std::string& d)
{
	defines.erase(d);
	resources.materials.reloadShadersWithDefine(d);
}

const std::unordered_set<std::string>& ShaderDefines::getDefines() const
{
	return defines;
}
