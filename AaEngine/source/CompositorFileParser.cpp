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
		
		auto findStr = param.value + ":";
		for (const auto& t : info.textures)
		{
			if (t.first.starts_with(findStr))
				textures.push_back({ t.first, Compositor::UsageFlags(flags) });
		}

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

					if (member.params.size() >= 3)
					{
						size_t idx = 0;

						if (member.params[idx].starts_with("target_size_div"))
						{
							tex.width = tex.height = 1 / std::stof(member.params[idx + 1]);
							tex.targetScale = true;
							idx += 2;
						}
						else
						{
							if (member.params[idx].starts_with("target_width_scaled"))
							{
								tex.width = std::stof(member.params[idx + 1]);
								tex.targetScale = true;
								idx += 2;
							}
							else if (member.params[idx].starts_with("target_width"))
							{
								tex.width = 1.f;
								tex.targetScale = true;
								idx += 1;
							}
							else
							{
								tex.width = std::stof(member.params[idx]);
								idx += 1;
							}

							if (member.params[idx].starts_with("target_height_scaled"))
							{
								tex.height = std::stof(member.params[idx + 1]);
								tex.targetScale = true;
								idx += 2;
							}
							else if (member.params[idx].starts_with("target_height"))
							{
								tex.height = 1.f;
								tex.targetScale = true;
								idx += 1;
							}
							else
							{
								tex.height = std::stof(member.params[idx]);
								idx += 1;
							}
						}

						std::vector<DXGI_FORMAT> formats;
						for (; idx < member.params.size(); idx++)
						{
							if (member.params[idx].starts_with("DXGI_FORMAT"))
								formats.push_back(StringToFormat(member.params[idx]));
							else if (member.params[idx] == "Depth")
								info.textures.emplace(tex.name + ":Depth", tex);
							else if (member.params[idx].starts_with('[') && member.params[idx].ends_with(']'))
								tex.arraySize = std::stoul(member.params[idx].c_str() + 1);
						}

						for (int i = 0; auto f : formats)
						{
							auto name = tex.name;
							if (formats.size() > 1)
								name += ":" + std::to_string(i++);

							tex.format = f;
							info.textures.emplace(name , tex);
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
							info.passes.push_back(pass);
						}
					}

					info.textures.insert(ref.textures.begin(), ref.textures.end());
					ref.textures.clear();
				}
			}

			return info;
		}
	}

	return {};
}
