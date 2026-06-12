#include "FrameCompositor/CompositorFileParser.h"
#include "Utils/ConfigParser.h"
#include "Utils/DxUtils.h"
#include <sstream>

static Compositor::UsageFlags parseFlags(const Config::Object& member)
{
	UINT flags = 0;
	for (auto& p : member.params)
	{
		if (p.starts_with('(') && p.ends_with(')'))
		{
			if (p == "(compute_shader)")
				flags |= Compositor::ComputeShader;
			else if (p == "(async_compute_queue)")
				flags |= Compositor::ComputeShader | Compositor::Async;
			else if (p == "(depth_read)")
				flags |= Compositor::DepthRead;
		}
	}
	if (member.type == "in" || member.type == "inout" || member.type == "outin")
		flags |= Compositor::Read;
	if (member.type == "out" || member.type == "inout" || member.type == "outin")
		flags |= Compositor::Write;
	if (member.type == "outin")
		flags |= Compositor::WriteFirst;
	if (member.type == "task" && !(flags & Compositor::ComputeShader))
		flags |= Compositor::PixelShader;

	return Compositor::UsageFlags(flags);
}

struct ParseContext
{
	std::string scope;
	std::map<std::string, std::string> externTextureNameRemap;
	std::map<std::string, std::string> textureAlias;
};

static std::vector<CompositorTextureSlot> parseCompositorTextureSlot(const Config::Object& param, const CompositorInfo& info, const ParseContext& ctx, UINT flags = 0)
{
	UINT textureFlags = parseFlags(param);
	UINT depthFlags = textureFlags & Compositor::DepthRead;
	textureFlags &= ~Compositor::DepthRead;

	std::vector<CompositorTextureSlot> textures;

	auto appendTexture = [&](const std::string& value)
		{
			if (value.starts_with('('))
				return;

			auto mrt = info.mrt.find(ctx.scope + value);
			if (mrt != info.mrt.end())
			{
				for (auto& t : mrt->second)
				{
					textures.emplace_back(t, Compositor::UsageFlags(flags));
				}
			}
			else
				textures.emplace_back(value, Compositor::UsageFlags(flags));
		};

	appendTexture(param.value);

	for (auto& p : param.params)
		appendTexture(p);

	for (auto& t : textures)
	{
		if (auto it = ctx.externTextureNameRemap.find(t.name); it != ctx.externTextureNameRemap.end())
			t.name = it->second;
		else if (auto it = ctx.textureAlias.find(t.name); it != ctx.textureAlias.end())
		{
			t.name = it->second;
		}
		else if (t.name.find('.') == std::string::npos)
			t.name = ctx.scope + t.name;

		if (t.name.ends_with("Depth") && depthFlags)
			t.flags = Compositor::UsageFlags(flags | depthFlags);
		else
			t.flags = Compositor::UsageFlags(flags | textureFlags);
	}

	return textures;
}

struct ParsedRef
{
	std::string name;
	std::vector<std::string> parameters;
};

static ParsedRef parseRef(const std::string& refString, const ParseContext& ctx)
{
	auto openParen = refString.find('(');
	auto closeParen = refString.find(')', openParen);

	if (openParen == std::string::npos || closeParen == std::string::npos)
		return { refString };

	ParsedRef result;
	result.name = refString.substr(0, openParen);
	std::string paramsStr = refString.substr(openParen + 1, closeParen - openParen - 1);

	std::stringstream ss(paramsStr);
	std::string param;
	while (std::getline(ss, param, ','))
	{
		if (auto it = ctx.textureAlias.find(param); it != ctx.textureAlias.end())
			param = it->second;

		if (!param.empty())
			result.parameters.push_back(param);
	}

	return result;
}

CompositorInfo CompositorFileParser::parseFile(std::string directory, std::string path, const Input& input)
{
	auto objects = Config::Parse(directory + path, input.defines);

	std::map<std::string, std::string> importMap;
	ParseContext ctx;
	ctx.scope = input.scope;

	for (auto& obj : objects)
	{
		if (obj.type == "import")
		{
			auto split = obj.value.find('.');
			auto from = obj.value.substr(0, split);
			auto what = obj.value.substr(split + 1);
			importMap[what] = from;
		}
		if (obj.type == "compositor")
		{
			CompositorInfo info;
			info.name = obj.value;

			for (auto& member : obj.children)
			{
				if (member.type == "extern")
				{
					ctx.externTextureNameRemap.emplace(member.value, input.textures[ctx.externTextureNameRemap.size()]);
				}
				if (member.type == "alias")
				{
					const auto& aliasTarget = member.params.front();
					if (auto it = ctx.textureAlias.find(aliasTarget); it == ctx.textureAlias.end())
						ctx.textureAlias.emplace(member.value, aliasTarget);
					else
						ctx.textureAlias.emplace(member.value, it->second);
				}
				if (member.type == "texture" || member.type == "rwtexture")
				{
					CompositorTextureInfo tex;
					tex.name = ctx.scope + member.value;
					tex.uav = member.type == "rwtexture";

					if (member.params.size() >= 2)
					{
						size_t idx = 0;
						std::string_view sizeParam = member.params[idx];
						if (sizeParam.starts_with("target_"))
							tex.targetScale = true;
						if (sizeParam.starts_with("output_"))
							tex.outputScale = true;

						if (tex.targetScale || tex.outputScale)
						{
							sizeParam = sizeParam.substr(7);

							if (sizeParam == "size")
							{
								tex.width = tex.height = 1;
								idx++;
							}
							else if (sizeParam == "size_div")
							{
								tex.width = tex.height = 1 / std::stof(member.params[++idx]);
							}
							else if (sizeParam == "size_scaled")
							{
								tex.width = tex.height = std::stof(member.params[++idx]);
							}
						}
						else
						{
							tex.width = std::stof(member.params[idx++]);
							tex.height = std::stof(member.params[idx++]);
						}

						std::vector<std::pair<DXGI_FORMAT, std::string>> formats;
						for (; idx < member.params.size(); idx++)
						{
							auto& param = member.params[idx];
							if (param.find("DXGI_FORMAT") != std::string::npos)
							{
								auto aliasEnd = param.find(':');
								if (aliasEnd == std::string::npos)
									formats.emplace_back(StringToDxgiFormat(param), std::string{});
								else
									formats.emplace_back(StringToDxgiFormat(param.substr(aliasEnd + 1)), param.substr(0, aliasEnd));
							}
							else if (param == "Depth")
								formats.push_back({ {}, "Depth" });
							else if (param.starts_with('[') && param.ends_with(']'))
								tex.arraySize = std::stoul(param.c_str() + 1);
						}

						if (formats.size() == 1)
						{
							tex.format = formats.front().first;
							info.textures.emplace(tex.name, tex);
						}
						else
						{
							auto& mrtOrder = info.mrt[tex.name];

							for (int i = 0; auto & f : formats)
							{
								auto name = tex.name + ":" + (f.second.empty() ? std::to_string(i++) : f.second);
								mrtOrder.push_back(name);
								tex.format = f.first;
								info.textures.emplace(name, tex);
							}
						}
					}
				}
				else if (member.type == "pass")
				{
					CompositorPassInfo pass;
					pass.name = member.value;
					pass.flags = Compositor::PixelShader;

					for (auto& param : member.children)
					{
						if (param.type == "out")
						{
							pass.targets = parseCompositorTextureSlot(param, info, ctx, Compositor::PixelShader);
						}
						else if (param.type == "material")
						{
							pass.material = param.value;
						}
						else if (param.type == "in")
						{
							pass.inputs.push_back(parseCompositorTextureSlot(param, info, ctx, Compositor::PixelShader).front());
						}
						else if (param.type == "sync_wait")
						{
							pass.sync.emplace_back(param.value, false, false);
						}
						else if (param.type == "sync_signal")
						{
							pass.sync.emplace_back(param.value, true, false);
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "task")
				{
					CompositorPassInfo pass;
					pass.name = member.value;

					if (auto pos = pass.name.find_first_of('.'); pos != std::string::npos)
					{
						pass.task = pass.name.substr(0, pos);
						pass.entry = pass.name.substr(pos + 1);
					}
					else
						pass.task = pass.name;

					pass.flags = parseFlags(member);

					for (auto& param : member.children)
					{
						if (param.type == "out")
						{
							pass.targets = parseCompositorTextureSlot(param, info, ctx, pass.flags);
							pass.mrt = pass.targets.size() > 1;
						}
						else if (param.type == "after")
						{
							pass.after = param.value;
						}
						else if (param.type == "sync_wait")
						{
							pass.sync.emplace_back(param.value, false, pass.flags& Compositor::Async);
						}
						else if (param.type == "sync_signal")
						{
							pass.sync.emplace_back(param.value, true, pass.flags & Compositor::Async);
						}
						else if (param.type == "in" || param.type == "inout" || param.type == "outin")
						{
							pass.inputs.push_back(parseCompositorTextureSlot(param, info, ctx, pass.flags).front());
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "ref")
				{
					const auto refInfo = parseRef(member.value, ctx);
					const auto source = importMap[refInfo.name];
					const auto refScope = input.scope + refInfo.name + ".";
					auto ref = parseFile(directory, source + ".compositor", { input.defines, refScope, refInfo.parameters });

					info.textures.insert(ref.textures.begin(), ref.textures.end());
					ref.textures.clear();

					for (auto& pass : ref.passes)
						info.passes.emplace_back(pass);
				}
			}

			return info;
		}
	}

	return {};
}
