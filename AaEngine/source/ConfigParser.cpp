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

static bool skipThisLine(const std::vector<std::string>& tokens, ParseContext& ctx)
{
	if (tokens.empty() || !tokens.front().starts_with('#'))
		return !ctx.ifdefs.empty() && !ctx.ifdefs.back().eval;

	auto& type = tokens.front();
	std::string value = tokens.size() > 1 ? tokens[1] : "";

	if (type == "#if" || type == "#ifdef")
	{
		ctx.ifdefs.emplace_back().eval = ctx.defines.contains(value);
		ctx.ifdefs.back().hadEval = ctx.ifdefs.back().eval;
	}
	else if (type == "#ifndef")
	{
		ctx.ifdefs.emplace_back().eval = !ctx.defines.contains(value);
		ctx.ifdefs.back().hadEval = ctx.ifdefs.back().eval;
	}
	else if (type == "#elif")
	{
		ctx.ifdefs.back().eval = ctx.defines.contains(value);
		ctx.ifdefs.back().hadEval |= ctx.ifdefs.back().eval;
	}
	else if (type == "#else")
	{
		ctx.ifdefs.back().eval = !ctx.ifdefs.back().hadEval;
	}
	else if (type == "#endif")
	{
		ctx.ifdefs.pop_back();
	}

	return true;
}

static std::vector<std::string> tokenize(const std::string& input)
{
	std::vector<std::string> tokens;
	std::string current;
	bool inParens = false;

	for (size_t i = 0; i < input.size(); ++i)
	{
		char c = input[i];

		// Handle comments: skip rest of line
		if (!inParens && c == '/' && i + 1 < input.size() && input[i + 1] == '/') {
			break; // stop processing
		}

		// Enter parentheses mode
		if (c == '(' && !inParens) {
			inParens = true;
		}

		// Exit parentheses mode
		if (c == ')' && inParens) {
			current.push_back(c);
			// remove whitespace inside parentheses
			std::string compact;
			for (char pc : current) {
				if (!std::isspace(static_cast<unsigned char>(pc))) {
					compact.push_back(pc);
				}
			}
			tokens.push_back(compact);
			current.clear();
			inParens = false;
			continue;
		}

		if (inParens) {
			current.push_back(c);
		}
		else {
			if (std::isspace(static_cast<unsigned char>(c))) {
				if (!current.empty()) {
					tokens.push_back(current);
					current.clear();
				}
			}
			else {
				current.push_back(c);
			}
		}
	}

	if (!current.empty()) {
		tokens.push_back(current);
	}

	return tokens;
}

static bool readNextObject(Config::Object& out, std::ifstream& file, ParseContext& ctx)
{
	std::string line;
	while (file && std::getline(file, line))
	{
		auto tokens = tokenize(line);

		if (skipThisLine(tokens, ctx))
			continue;

		if (!tokens.empty())
		{
			out.type = tokens.front();
			if (tokens.size() > 1)
				out.value = tokens[1];
			if (tokens.size() > 2)
				out.params.assign(tokens.begin() + 2, tokens.end());

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
