#include "ShaderDefines.h"
#include "GraphicsResources.h"

ShaderDefines::ShaderDefines(GraphicsResources& r) : resources(r)
{
}

void ShaderDefines::setDefine(const std::string& d, bool enabled)
{
	bool changed = (enabled && defines.insert(d).second) || (!enabled && defines.erase(d));

	if (changed)
		resources.materials.reloadShadersWithDefine(d);
}

const std::unordered_set<std::string>& ShaderDefines::getDefines() const
{
	return defines;
}
