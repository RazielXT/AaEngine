#include "CompositorFileParser.h"
#include "ConfigParser.h"
#include <sstream>

static DXGI_FORMAT StringToFormat(const std::string& str)
{
	std::map<std::string, DXGI_FORMAT> dxgiFormatMap = {
		{"DXGI_FORMAT_UNKNOWN", DXGI_FORMAT_UNKNOWN},
		{"DXGI_FORMAT_R32G32B32A32_TYPELESS", DXGI_FORMAT_R32G32B32A32_TYPELESS},
		{"DXGI_FORMAT_R32G32B32A32_FLOAT", DXGI_FORMAT_R32G32B32A32_FLOAT},
		{"DXGI_FORMAT_R32G32B32A32_UINT", DXGI_FORMAT_R32G32B32A32_UINT},
		{"DXGI_FORMAT_R32G32B32A32_SINT", DXGI_FORMAT_R32G32B32A32_SINT},
		{"DXGI_FORMAT_R32G32B32_TYPELESS", DXGI_FORMAT_R32G32B32_TYPELESS},
		{"DXGI_FORMAT_R32G32B32_FLOAT", DXGI_FORMAT_R32G32B32_FLOAT},
		{"DXGI_FORMAT_R32G32B32_UINT", DXGI_FORMAT_R32G32B32_UINT},
		{"DXGI_FORMAT_R32G32B32_SINT", DXGI_FORMAT_R32G32B32_SINT},
		{"DXGI_FORMAT_R16G16B16A16_TYPELESS", DXGI_FORMAT_R16G16B16A16_TYPELESS},
		{"DXGI_FORMAT_R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT},
		{"DXGI_FORMAT_R16G16B16A16_UNORM", DXGI_FORMAT_R16G16B16A16_UNORM},
		{"DXGI_FORMAT_R16G16B16A16_UINT", DXGI_FORMAT_R16G16B16A16_UINT},
		{"DXGI_FORMAT_R16G16B16A16_SNORM", DXGI_FORMAT_R16G16B16A16_SNORM},
		{"DXGI_FORMAT_R16G16B16A16_SINT", DXGI_FORMAT_R16G16B16A16_SINT},
		{"DXGI_FORMAT_R32G32_TYPELESS", DXGI_FORMAT_R32G32_TYPELESS},
		{"DXGI_FORMAT_R32G32_FLOAT", DXGI_FORMAT_R32G32_FLOAT},
		{"DXGI_FORMAT_R32G32_UINT", DXGI_FORMAT_R32G32_UINT},
		{"DXGI_FORMAT_R32G32_SINT", DXGI_FORMAT_R32G32_SINT},
		{"DXGI_FORMAT_R32G8X24_TYPELESS", DXGI_FORMAT_R32G8X24_TYPELESS},
		{"DXGI_FORMAT_D32_FLOAT_S8X24_UINT", DXGI_FORMAT_D32_FLOAT_S8X24_UINT},
		{"DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS", DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS},
		{"DXGI_FORMAT_X32_TYPELESS_G8X24_UINT", DXGI_FORMAT_X32_TYPELESS_G8X24_UINT},
		{"DXGI_FORMAT_R10G10B10A2_TYPELESS", DXGI_FORMAT_R10G10B10A2_TYPELESS},
		{"DXGI_FORMAT_R10G10B10A2_UNORM", DXGI_FORMAT_R10G10B10A2_UNORM},
		{"DXGI_FORMAT_R10G10B10A2_UINT", DXGI_FORMAT_R10G10B10A2_UINT},
		{"DXGI_FORMAT_R11G11B10_FLOAT", DXGI_FORMAT_R11G11B10_FLOAT},
		{"DXGI_FORMAT_R8G8B8A8_TYPELESS", DXGI_FORMAT_R8G8B8A8_TYPELESS},
		{"DXGI_FORMAT_R8G8B8A8_UNORM", DXGI_FORMAT_R8G8B8A8_UNORM},
		{"DXGI_FORMAT_R8G8B8A8_UNORM_SRGB", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
		{"DXGI_FORMAT_R8G8B8A8_UINT", DXGI_FORMAT_R8G8B8A8_UINT},
		{"DXGI_FORMAT_R8G8B8A8_SNORM", DXGI_FORMAT_R8G8B8A8_SNORM},
		{"DXGI_FORMAT_R8G8B8A8_SINT", DXGI_FORMAT_R8G8B8A8_SINT},
		{"DXGI_FORMAT_R16G16_TYPELESS", DXGI_FORMAT_R16G16_TYPELESS},
		{"DXGI_FORMAT_R16G16_FLOAT", DXGI_FORMAT_R16G16_FLOAT},
		{"DXGI_FORMAT_R16G16_UNORM", DXGI_FORMAT_R16G16_UNORM},
		{"DXGI_FORMAT_R16G16_UINT", DXGI_FORMAT_R16G16_UINT},
		{"DXGI_FORMAT_R16G16_SNORM", DXGI_FORMAT_R16G16_SNORM},
		{"DXGI_FORMAT_R16G16_SINT", DXGI_FORMAT_R16G16_SINT},
		{"DXGI_FORMAT_R32_TYPELESS", DXGI_FORMAT_R32_TYPELESS},
		{"DXGI_FORMAT_D32_FLOAT", DXGI_FORMAT_D32_FLOAT},
		{"DXGI_FORMAT_R32_FLOAT", DXGI_FORMAT_R32_FLOAT},
		{"DXGI_FORMAT_R32_UINT", DXGI_FORMAT_R32_UINT},
		{"DXGI_FORMAT_R32_SINT", DXGI_FORMAT_R32_SINT},
		{"DXGI_FORMAT_R16_UNORM", DXGI_FORMAT_R16_UNORM},
		{"DXGI_FORMAT_R16_FLOAT", DXGI_FORMAT_R16_FLOAT},
		{"DXGI_FORMAT_R8_UNORM", DXGI_FORMAT_R8_UNORM},
		{"DXGI_FORMAT_R24G8_TYPELESS", DXGI_FORMAT_R24G8_TYPELESS}
	};

	auto it = dxgiFormatMap.find(str);
	if (it != dxgiFormatMap.end())
		return it->second;

	return DXGI_FORMAT_UNKNOWN;
}

static UINT parseFlags(const Config::Object& member)
{
	UINT flags = 0;
	for (auto& p : member.params)
	{
		if (p.starts_with('(') && p.ends_with(')'))
		{
			if (p == "(compute_shader)")
				flags |= Compositor::ComputeShader;
			else if (p == "(depth_read)")
				flags |= Compositor::DepthRead;
			else if (p == "(read)")
				flags |= Compositor::Read;
		}
	}
	return flags;
}

struct ParseContext
{
	std::string scope;
	std::map<std::string, std::string> externTextureNameRemap;
};

static std::vector<CompositorTextureSlot> parseCompositorTextureSlot(const Config::Object& param, const CompositorInfo& info, const ParseContext& ctx, UINT flags = 0)
{
	flags |= parseFlags(param);
	if (!flags)
		flags = Compositor::PixelShader;

	std::vector<CompositorTextureSlot> textures;

	auto appendTexture = [&](const std::string& value)
		{
			if (value.starts_with('('))
				return;

			auto mrt = info.mrt.find(value);
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
		else
			t.name = ctx.scope + t.name;
	}

	return textures;
}

struct ParsedRef
{
	std::string name;
	std::vector<std::string> parameters;
};

static ParsedRef parseRef(const std::string& refString)
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
									formats.emplace_back(StringToFormat(param), std::string{});
								else
									formats.emplace_back(StringToFormat(param.substr(aliasEnd + 1)), param.substr(0, aliasEnd));
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
							std::vector<std::string> mrtOrder;

							for (int i = 0; auto & f : formats)
							{
								auto name = tex.name + ":" + (f.second.empty() ? std::to_string(i++) : f.second);
								mrtOrder.push_back(name);
								tex.format = f.first;
								info.textures.emplace(name, tex);
							}

							info.mrt[tex.name] = mrtOrder;
						}
					}
				}
				else if (member.type == "pass")
				{
					CompositorPassInfo pass;
					pass.name = member.value;

					for (auto& param : member.children)
					{
						if (param.type == "target")
						{
							pass.targets = parseCompositorTextureSlot(param, info, ctx);
						}
						else if (param.type == "material")
						{
							pass.material = param.value;
						}
						else if (param.type == "input")
						{
							pass.inputs.push_back(parseCompositorTextureSlot(param, info, ctx).front());
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "task")
				{
					CompositorPassInfo pass;
					pass.task = pass.name = member.value;

					UINT flags = parseFlags(member);

					for (auto& param : member.children)
					{
						if (param.type == "target")
						{
							pass.targets = parseCompositorTextureSlot(param, info, ctx, flags);
						}
						else if (param.type == "entry")
						{
							pass.entry = param.value;
						}
						else if (param.type == "after")
						{
							pass.after = param.value;
						}
						else if (param.type == "input")
						{
							pass.inputs.push_back(parseCompositorTextureSlot(param, info, ctx, flags).front());
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "ref")
				{
					const auto refInfo = parseRef(member.value);
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
