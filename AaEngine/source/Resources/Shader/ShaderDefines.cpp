#include "Resources/Shader/ShaderDefines.h"
#include "Resources/GraphicsResources.h"

ShaderDefines::ShaderDefines(GraphicsResources& r) : resources(r)
{
}

void ShaderDefines::setDefine(const std::string& d, bool enabled)
{
	bool changed = (enabled && defines.emplace(d, "").second) || (!enabled && defines.erase(d));

	if (changed)
		resources.materials.reloadShadersWithDefine(d);
}

void ShaderDefines::setDefine(const std::string& d, const std::string& value)
{
	defines[d] = value;

	resources.materials.reloadShadersWithDefine(d);
}

const std::unordered_map<std::string, std::string>& ShaderDefines::getDefines() const
{
	return defines;
}
