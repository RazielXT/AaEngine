#pragma once

#include <string>
#include <memory>
#include <DirectXCollision.h>
#include <format>
#include <SimpleMath.h>
#include "AaModel.h"
#include "d3d12.h"

using namespace DirectX::SimpleMath;

namespace OgreMeshFileParser
{
	struct ParseOptions
	{
		ResourceUploadBatch* batch;
		ID3D12Device* device;
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
