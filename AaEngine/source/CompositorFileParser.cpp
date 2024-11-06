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
		{"DXGI_FORMAT_R24G8_TYPELESS", DXGI_FORMAT_R24G8_TYPELESS}
	};

	auto it = dxgiFormatMap.find(str);
	if (it != dxgiFormatMap.end())
		return it->second;

	return DXGI_FORMAT_UNKNOWN;
}

CompositorInfo CompositorFileParser::parseFile(std::string path)
{
	auto objects = Config::Parse(path);

	for (auto& obj : objects)
	{
		if (obj.type == "compositor")
		{
			CompositorInfo info;
			info.name = obj.value;

			for (auto& member : obj.children)
			{
				if (member.type == "texture")
				{
					CompositorTexture tex;
					tex.name = member.value;

					if (member.params.size() >= 3)
					{
						size_t idx = 0;
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

						for (; idx < member.params.size(); idx++)
						{
							if (member.params[idx].starts_with("DXGI_FORMAT"))
								tex.formats.push_back(StringToFormat(member.params[idx]));
							if (member.params[idx] == "depth_buffer")
								tex.depthBuffer = true;
						}
					}

					info.textures.push_back(tex);
				}
				else if (member.type == "pass")
				{
					CompositorPass pass;
					pass.name = member.value;

					for (auto& param : member.children)
					{
						if (param.type == "target")
						{
							pass.target = param.value;

							if (!param.params.empty())
								pass.targetIndex = std::stoul(param.params.front());
						}
						else if (param.type == "material")
						{
							pass.material = param.value;
						}
						else if (param.type == "input")
						{
							UINT index = param.params.empty() ? 0 : std::stoul(param.params.front());
							pass.inputs.emplace_back(param.value, index);
						}
					}

					info.passes.push_back(pass);
				}
				else if (member.type == "render")
				{
					CompositorPass pass;
					pass.render = pass.name = member.value;

					for (auto& param : member.children)
					{
						if (param.type == "target")
						{
							pass.target = param.value;

							if (!param.params.empty())
								pass.targetIndex = std::stoul(param.params.front());
						}
						else if (param.type == "after")
						{
							pass.after = param.value;
						}
						else if (param.type == "params")
						{
							pass.params = { param.value };
						}
					}

					info.passes.push_back(pass);
				}
			}

			return info;
		}
	}

	return {};
}
