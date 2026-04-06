#include "ShaderType.h"

namespace ShaderTypeString
{
	ShaderType Parse(const std::string& t)
	{
		ShaderType type = ShaderType::None;
		if (t == "vertex_shader")
			type = ShaderType::Vertex;
		else if (t == "pixel_shader")
			type = ShaderType::Pixel;
		else if (t == "geometry_shader")
			type = ShaderType::Geometry;
		else if (t == "compute_shader")
			type = ShaderType::Compute;
		else if (t == "amplification_shader")
			type = ShaderType::Amplification;
		else if (t == "mesh_shader")
			type = ShaderType::Mesh;

		return type;
	}

	std::string ShortName(ShaderType type)
	{
		if (type == ShaderType::Vertex)
			return "vs";
		if (type == ShaderType::Pixel)
			return "ps";
		if (type == ShaderType::Geometry)
			return "gs";
		if (type == ShaderType::Compute)
			return "cs";
		if (type == ShaderType::Amplification)
			return "as";
		if (type == ShaderType::Mesh)
			return "ms";

		return "";
	}
};
