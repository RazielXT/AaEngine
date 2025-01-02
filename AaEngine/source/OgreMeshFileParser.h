#pragma once

#include <string>
#include <memory>
#include <DirectXCollision.h>
#include <format>
#include <SimpleMath.h>
#include "VertexBufferModel.h"
#include "ModelParseOptions.h"

using namespace DirectX::SimpleMath;

namespace OgreMeshFileParser
{
	struct SubmeshInfo
	{
		VertexBufferModel* model;

		std::string name;
		std::string materialName;
	};
	struct MeshInfo
	{
		std::vector<SubmeshInfo> submeshes;

		DirectX::BoundingBox boundingBox;
		DirectX::BoundingSphere boundingSphere;
	};

	MeshInfo load(std::string filename, ModelParseOptions);
};
