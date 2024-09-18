#include "ConfigParser.h"
#include <fstream>

static bool readNextObject(Config::Object& obj, std::ifstream& file)
{
	std::string line;
	while (file && std::getline(file, line))
	{
		size_t pos = 0;

		while (pos < line.size())
		{
			while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
				pos++;

			auto start = pos;
			if (pos <= line.size())
			{
				while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
					pos++;

				auto token = line.substr(start, pos - start);
				if (token.starts_with("//"))
					break;

				if (obj.type.empty())
					obj.type = token;
				else if (obj.value.empty())
					obj.value = token;
				else
					obj.params.push_back(token);
			}
		}

		if (!obj.type.empty())
			return true;
	}
	
	return false;
}

static bool parseObjects(std::ifstream& file, Config::Object& parent)
{
	while (file)
	{
		Config::Object obj;

		if (!readNextObject(obj, file))
			return true;

		if (obj.type == "{")
		{
			if (parent.children.empty() || !parseObjects(file, parent.children.back()))
				return false;
		}
		else if (obj.type == "}")
		{
			return true;
		}
		else
		{
			parent.children.push_back(obj);
		}
	}

	return true;
}

std::vector<Config::Object> Config::Parse(const std::string& path)
{
	std::ifstream file(path);

	Config::Object obj;
	if (!parseObjects(file, obj))
		return {};

	return obj.children;
}
