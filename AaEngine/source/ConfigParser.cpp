#include "ConfigParser.h"
#include <fstream>

namespace
{
	struct ParseContext
	{
		const std::set<std::string>& defines;

		struct IfdefScope
		{
			bool eval{};
			bool hadEval{};
		};
		std::vector<IfdefScope> ifdefs;
	};
}

static bool skipThisLine(const Config::Object& obj, ParseContext& ctx)
{
	if (!obj.type.starts_with('#'))
		return !ctx.ifdefs.empty() && !ctx.ifdefs.back().eval;

	if (obj.type == "#if" || obj.type == "#ifdef")
	{
		ctx.ifdefs.emplace_back().eval = ctx.defines.contains(obj.value);
		ctx.ifdefs.back().hadEval = ctx.ifdefs.back().eval;
	}
	else if (obj.type == "#ifndef")
	{
		ctx.ifdefs.emplace_back().eval = !ctx.defines.contains(obj.value);
		ctx.ifdefs.back().hadEval = ctx.ifdefs.back().eval;
	}
	else if (obj.type == "#elif")
	{
		ctx.ifdefs.back().eval = ctx.defines.contains(obj.value);
		ctx.ifdefs.back().hadEval |= ctx.ifdefs.back().eval;
	}
	else if (obj.type == "#else")
	{
		ctx.ifdefs.back().eval = !ctx.ifdefs.back().hadEval;
	}
	else if (obj.type == "#endif")
	{
		ctx.ifdefs.pop_back();
	}

	return true;
}

static bool readNextObject(Config::Object& out, std::ifstream& file, ParseContext& ctx)
{
	std::string line;
	while (file && std::getline(file, line))
	{
		size_t pos = 0;
		Config::Object obj;

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

		if (skipThisLine(obj, ctx))
			continue;

		if (!obj.type.empty())
		{
			out = obj;
			return true;
		}
	}

	return false;
}

static bool parseObjects(std::ifstream& file, Config::Object& parent, ParseContext& ctx)
{
	while (file)
	{
		Config::Object obj;

		if (!readNextObject(obj, file, ctx))
			return true;

		if (obj.type == "{")
		{
			if (parent.children.empty() || !parseObjects(file, parent.children.back(), ctx))
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

std::vector<Config::Object> Config::Parse(const std::string& path, const std::set<std::string>& defines)
{
	std::ifstream file(path);

	ParseContext ctx = { defines };
	Config::Object obj;
	if (!parseObjects(file, obj, ctx))
		return {};

	return obj.children;
}
