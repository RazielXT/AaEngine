#pragma once

#include <string>
#include <unordered_set>

struct GraphicsResources;

class ShaderDefines
{
public:

	ShaderDefines(GraphicsResources&);

	void setDefine(const std::string&, bool enabled);
	const std::unordered_set<std::string>& getDefines() const;

private:

	std::unordered_set<std::string> defines;

	GraphicsResources& resources;
};
