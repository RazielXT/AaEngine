#pragma once

#include "AaMaterial.h"
#include <vector>
#include <map>

enum class TextureBorder
{
	Warp, Clamp, BorderColor
};

enum class Filtering
{
	None, Bilinear, Trilinear, Anisotropic
};

struct textureInfo
{
	std::string id;
	std::string file;
	std::string name;
	bool defaultSampler = true;
	int maxAnisotropy = 8;
	Filtering filter = Filtering::Anisotropic;
	TextureBorder bordering = TextureBorder::Warp;
	float border_color[4]{};
};

struct materialInfo
{
	std::string name;
	std::string ps_ref;
	std::string vs_ref;
	std::vector<textureInfo> vstextures;
	std::vector<textureInfo> pstextures;
	std::vector<std::string> psuavs;

	std::map<std::string, std::vector<float>> defaultParams;
	RS_DESC renderStateDesc;
};

namespace AaMaterialFileParser
{
	std::vector<materialInfo> parseAllMaterialFiles(std::string directory, bool subFolders = false);
};
