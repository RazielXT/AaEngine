#pragma once

#include <string>
#include <unordered_map>

struct GraphicsResources;

class ShaderDefines
{
public:

	ShaderDefines(GraphicsResources&);

	void setDefine(const std::string&, bool enabled);
	void setDefine(const std::string&, const std::string&);
	const std::unordered_map<std::string, std::string>& getDefines() const;

private:

	std::unordered_map<std::string, std::string> defines;

	GraphicsResources& resources;
};
