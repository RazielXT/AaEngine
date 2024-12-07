#include "CompositorFileParser.h"
#include "ConfigParser.h"

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
		}
	}
	return flags;
}

static std::vector<CompositorTextureSlot> parseCompositorTextureSlot(const Config::Object& param, const CompositorInfo& info, UINT flags = 0)
{
	flags |= parseFlags(param);
	if (!flags)
		flags = Compositor::PixelShader;

	std::vector<CompositorTextureSlot> textures;

	if (param.params.empty())
	{
		if (info.textures.contains(param.value) || param.value == "Backbuffer")
			return { { param.value, Compositor::UsageFlags(flags) } };
		
		auto mrt = info.mrt.find(param.value);
		if (mrt != info.mrt.end())
		{
			for (auto& t : mrt->second)
			{
				textures.push_back({ t, Compositor::UsageFlags(flags) });
			}
		}

		if (textures.empty())
			return { { param.value, Compositor::UsageFlags(flags) } };

		return textures;
	}

	for (auto& p : param.params)
	{
		if (!p.starts_with('('))
			textures.push_back({ param.value + ":" + p, Compositor::UsageFlags(flags) });
	}
	return textures;
}

CompositorInfo CompositorFileParser::parseFile(std::string directory, std::string path)
{
	auto objects = Config::Parse(directory + path);
	std::map<std::string, CompositorInfo> imports;
	std::optional<CompositorPassCondition> currentCondition;

	for (auto& obj : objects)
	{
		if (obj.type == "import")
		{
			auto parsedImport = parseFile(directory, obj.value + ".compositor");
			imports[parsedImport.name] = std::move(parsedImport);
		}
		if (obj.type == "compositor")
		{
			CompositorInfo info;
			info.name = obj.value;

			for (auto& member : obj.children)
			{
				if (member.type == "texture" || member.type == "rwtexture")
				{
					CompositorTextureInfo tex;
					tex.name = member.value;
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
					pass.condition = currentCondition;

					for (auto& param : member.children)
					{
						if (param.type == "target")
						{
							pass.targets = parseCompositorTextureSlot(param, info);
						}
						else if (param.type == "material")
						{
							pass.material = param.value;
						}
						else if (param.type == "input")
						{
							pass.inputs.push_back(parseCompositorTextureSlot(param, info).front());
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "task")
				{
					CompositorPassInfo pass;
					pass.task = pass.name = member.value;
					pass.condition = currentCondition;

					UINT flags = parseFlags(member);

					for (auto& param : member.children)
					{
						if (param.type == "target")
						{
							pass.targets = parseCompositorTextureSlot(param, info, flags);
						}
						else if (param.type == "after")
						{
							pass.after = param.value;
						}
						else if (param.type == "params")
						{
							pass.params = { param.value };
						}
						else if (param.type == "input")
						{
							pass.inputs.push_back(parseCompositorTextureSlot(param, info, flags).front());
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "ref")
				{
					auto split = member.value.find(':');
					auto from = member.value.substr(0, split);
					auto what = member.value.substr(split + 1);

					auto& ref = imports[from];
					for (auto& pass : ref.passes)
					{
						if (pass.name == what || what == "*")
						{
							auto& added = info.passes.emplace_back(pass);
							added.condition = currentCondition;
						}
					}

					info.textures.insert(ref.textures.begin(), ref.textures.end());
					ref.textures.clear();
				}
				else if (member.type.starts_with('#'))
				{
					if (member.type == "#if")
					{
						currentCondition = { true, member.value };
					}
					else if (member.type == "#else")
					{
						if (currentCondition)
							currentCondition->accept = false;
					}
					else if (member.type == "#endif")
					{
						currentCondition.reset();
					}
				}
			}

			return info;
		}
	}

	return {};
}
