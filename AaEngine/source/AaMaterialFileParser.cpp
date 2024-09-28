#include "AaMaterialFileParser.h"
#include "AaLogger.h"
#include <filesystem>
#include "ConfigParser.h"

static bool parseToggleValue(const std::string& value)
{
	return value == "on" || value == "1";
}

void parseMaterialFile(std::vector<MaterialRef>& mats, std::string file)
{
	auto objects = Config::Parse(file);

	for (auto& obj : objects)
	{
		if (obj.type == "material")
		{
			MaterialRef mat;

			if (obj.params.size() > 1 && obj.params[0] == ":")
			{
				auto parent = obj.params[1];
				if (!parent.empty())
				{
					for (auto& m : mats)
					{
						if (m.name == parent)
							mat = m;
					}
					if (mat.name.empty())
					{
						AaLogger::logWarning("Missing material base " + parent);
						continue;
					}

					if (mat.base.empty())
						mat.base = parent;
				}
			}
			if (!obj.params.empty())
				mat.abstract = obj.params.front() == "abstract";

			mat.name = obj.value;

			for (auto& member : obj.children)
			{
				bool parentInvalidated = true;

				if (member.type == "vertex_shader")
				{
					mat.pipeline.vs_ref = member.value;
				}
				else if (member.type == "pixel_shader")
				{
					mat.pipeline.ps_ref = member.value;
				}
				else if (member.type == "depth")
				{
					mat.pipeline.depth.check = mat.pipeline.depth.write = parseToggleValue(member.value);
				}
				else if (member.type == "depth_write")
				{
					mat.pipeline.depth.write = parseToggleValue(member.value);
				}
				else if (member.type == "depth_check")
				{
					mat.pipeline.depth.check = parseToggleValue(member.value);
				}
				else
				{
					parentInvalidated = false;
				}

				if (parentInvalidated)
					mat.base.clear();

				// resources
				if (member.type == "default_params")
				{
					//get const value
					for (auto& param : member.children)
					{
						std::vector<float> values;
						for (auto& value : param.params)
						{
							values.push_back(std::stof(value));
						}

						mat.resources.defaultParams[param.value] = values;
					}
				}
				else if (member.type == "UAV")
				{
					mat.resources.uavs.push_back(member.value);
				}
				else if (member.type.starts_with("texture"))
				{
					auto& texturesTarget = mat.resources.textures;

					std::optional<size_t> prevTextureIndex;
					if (!member.value.empty())
					{
						for (size_t i = 0; i < texturesTarget.size(); i++)
						{
							if (texturesTarget[i].id == member.value)
								prevTextureIndex = i;
						}
					}

					TextureRef newTex;
					TextureRef& tex = prevTextureIndex ? texturesTarget[*prevTextureIndex] : newTex;
					tex.id = member.value;

					for (auto& param : member.children)
					{
						if (param.type == "file")
						{
							tex.file = param.value;
						}
					}

					if (!prevTextureIndex)
						texturesTarget.push_back(tex);
				}
				else if (member.type.starts_with("sampler"))
				{
					SamplerRef sampler;
					member.params.push_back(member.value);

					for (auto& param : member.params)
					{
						if (param == "bilinear")
							sampler.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
						else if (param == "point")
							sampler.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
						else if (param == "wrap")
							sampler.bordering = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
						else if (param == "clamp")
							sampler.bordering = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
						else if (param == "border_black")
						{
							sampler.bordering = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
							sampler.borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
						}
						else if (param == "border_white")
						{
							sampler.bordering = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
							sampler.borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
						}
					}

					mat.resources.samplers.push_back(sampler);
				}
			}

			mats.push_back(mat);
		}
	}
}

void AaMaterialFileParser::parseAllMaterialFiles(std::vector<MaterialRef>& mats, std::string directory, bool subFolders)
{
	std::error_code ec;

	for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
	{
		if (entry.is_directory() && subFolders)
		{
			parseAllMaterialFiles(mats, entry.path().generic_string(), subFolders);
		}
		else if (entry.is_regular_file() && entry.path().generic_string().ends_with(".material"))
		{
			AaLogger::log("Parsing " + entry.path().generic_string());
			parseMaterialFile(mats, entry.path().generic_string());
		}
	}

	if (ec)
		AaLogger::logWarning(ec.message());
}
