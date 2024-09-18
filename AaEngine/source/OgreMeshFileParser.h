#pragma once

#include "AaModel.h"
#include <string>
#include <memory>
#include <DirectXCollision.h>

namespace OgreMeshFileParser
{
	struct ParseOptions
	{
		AaRenderSystem* rs;
	};

	struct SubmeshInfo
	{
		AaModel* model;

		std::string name;
		std::string materialName;
	};
	struct MeshInfo
	{
		std::vector<SubmeshInfo> submeshes;

		DirectX::BoundingBox boundingBox;
		DirectX::BoundingSphere boundingSphere;
	};

	MeshInfo load(std::string filename, ParseOptions);
};