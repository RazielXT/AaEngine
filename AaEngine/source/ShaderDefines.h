#pragma once

#include <string>
#include <unordered_set>

struct GraphicsResources;

class ShaderDefines
{
public:

	ShaderDefines(GraphicsResources&);

	void setDefine(const std::string&);
	void removeDefine(const std::string&);
	const std::unordered_set<std::string>& getDefines() const;

private:

	std::unordered_set<std::string> defines;

	GraphicsResources& resources;
};
