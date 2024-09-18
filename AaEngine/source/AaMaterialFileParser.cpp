#include "AaMaterialFileParser.h"
#include "AaLogger.h"
#include <filesystem>
#include "ConfigParser.h"

std::vector<materialInfo> parseMaterialFile(std::string file)
{
	std::vector<materialInfo> mats;

	auto objects = Config::Parse(file);

	for (auto& obj : objects)
	{
		if (obj.type == "material")
		{
			materialInfo mat;

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
				}
			}

			mat.name = obj.value;

			for (auto& member : obj.children)
			{
				if (member.type == "vertex_shader")
				{
					mat.vs_ref = member.value;
				}
				else if (member.type == "pixel_shader")
				{
					mat.ps_ref = member.value;
				}
				else if (member.type == "depth_write_off")
				{
					mat.renderStateDesc.depth_write = 0;
				}
				else if (member.type == "depth_check_off")
				{
					mat.renderStateDesc.depth_check = 0;
				}
				else if (member.type == "culling_none")
				{
					mat.renderStateDesc.culling = 0;
				}
				else if (member.type == "alpha_blend")
				{
					mat.renderStateDesc.alpha_blend = 1;
				}
				else if (member.type == "default_params")
				{
					//get const value
					for (auto& param : member.children)
					{
						std::vector<float> values;
						for (auto& value : param.params)
						{
							values.push_back(std::stof(value));
						}

						mat.defaultParams[param.value] = values;
					}
				}
				else if (member.type == "UAV")
				{
					mat.psuavs.push_back(member.value);
				}
				else if (member.type.starts_with("texture"))
				{
					bool psTexture = !member.type.ends_with("_vs");
					auto& texturesTarget = psTexture ? mat.pstextures : mat.vstextures;

					std::optional<size_t> prevTextureIndex;
					if (!member.value.empty())
					{
						for (size_t i = 0; i < texturesTarget.size(); i++)
						{
							if (texturesTarget[i].id == member.value)
								prevTextureIndex = i;
						}
					}

					textureInfo newTex;
					textureInfo& tex = prevTextureIndex ? texturesTarget[*prevTextureIndex] : newTex;
					tex.id = member.value;

					for (auto& param : member.children)
					{
						if (param.type == "file")
						{
							tex.file = param.value;
						}
						else if (param.type == "name")
						{
							tex.name = param.value;
						}
						else if (param.type == "filtering")
						{
							tex.defaultSampler = false;

							if (param.value == "bilinear")
								tex.filter = Filtering::Bilinear;
							else if (param.value == "none")
								tex.filter = Filtering::None;
							else
								tex.filter = Filtering::Anisotropic;
						}
						else if (param.type == "border")
						{
							tex.defaultSampler = false;

							if (param.value == "warp")
								tex.bordering = TextureBorder::Clamp;
							else if (param.value == "color" && param.params.size() >= 4)
							{
								tex.bordering = TextureBorder::BorderColor;
								for (size_t i = 0; i < 4; i++)
								{
									tex.border_color[i] = std::stof(param.params[i]);
								}
							}
							else
								tex.bordering = TextureBorder::Clamp;
						}
					}

					if (!prevTextureIndex)
						texturesTarget.push_back(tex);
				}
			}

			mats.push_back(mat);
		}
	}

	return mats;
}

std::vector<materialInfo> AaMaterialFileParser::parseAllMaterialFiles(std::string directory, bool subFolders)
{
	std::vector<materialInfo> mats;

	if (!std::filesystem::exists(directory))
		return mats;

	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		if (entry.is_directory() && subFolders)
		{
			std::vector<materialInfo> subMats = parseAllMaterialFiles(entry.path().generic_string(), subFolders);
			mats.insert(mats.end(), subMats.begin(), subMats.end());
		}
		else if (entry.is_regular_file() && entry.path().generic_string().ends_with(".material"))
		{
			AaLogger::log(entry.path().generic_string());
			std::vector<materialInfo> localMats = parseMaterialFile(entry.path().generic_string());
			mats.insert(mats.end(), localMats.begin(), localMats.end());
			AaLogger::log("Loaded materials: " + std::to_string(localMats.size()));
		}
	}

	AaLogger::log("Total mats here: " + std::to_string(mats.size()));
	return mats;
}
