#pragma once

#include <string>
#include <vector>
#include <set>

namespace Config
{
	struct Object
	{
		std::string type;
		std::string value;

		std::vector<std::string> params;
		std::vector<Object> children;
	};

	std::vector<Object> Parse(const std::string& file, const std::set<std::string>& defines = {});
}
