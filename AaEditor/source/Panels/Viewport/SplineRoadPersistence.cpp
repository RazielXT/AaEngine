#include "SplineRoadPersistence.h"

#include "SplineRoadTool.h"
#include "App/Directories.h"
#include "Resources/Model/BinaryModelSerialization.h"
#include "Utils/Logger.h"

#define PUGIXML_HEADER_ONLY
#include "pugixml.hpp"

#include <Windows.h>
#include <commdlg.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace
{
	const char* profileToString(SplineRoadTool::PreviewProfile profile)
	{
		switch (profile)
		{
		case SplineRoadTool::PreviewProfile::Rectangle:
			return "Rectangle";
		case SplineRoadTool::PreviewProfile::Circle:
			return "Circle";
		case SplineRoadTool::PreviewProfile::Tube:
			return "Tube";
		case SplineRoadTool::PreviewProfile::Arc:
			return "Arc";
		case SplineRoadTool::PreviewProfile::FilledArc:
			return "FilledArc";
		case SplineRoadTool::PreviewProfile::Road:
		default:
			return "Road";
		}
	}

	SplineRoadTool::PreviewProfile profileFromString(const char* value)
	{
		std::string profile = value ? value : "";
		if (profile == "Rectangle")
			return SplineRoadTool::PreviewProfile::Rectangle;
		if (profile == "Circle")
			return SplineRoadTool::PreviewProfile::Circle;
		if (profile == "Tube")
			return SplineRoadTool::PreviewProfile::Tube;
		if (profile == "Arc")
			return SplineRoadTool::PreviewProfile::Arc;
		if (profile == "FilledArc")
			return SplineRoadTool::PreviewProfile::FilledArc;
		return SplineRoadTool::PreviewProfile::Road;
	}

	std::filesystem::path defaultSplineExportDirectory()
	{
		return std::filesystem::path(DATA_DIRECTORY) / "scene" / "export" / "spline";
	}

	std::string makeTimestampSuffix()
	{
		const auto now = std::chrono::system_clock::now();
		const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
		std::tm localTime{};
		localtime_s(&localTime, &nowTime);

		std::ostringstream stream;
		stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
		return stream.str();
	}

	std::filesystem::path makeTimestampedExportPath(const char* prefix, const char* extension)
	{
		std::filesystem::path directory = defaultSplineExportDirectory();
		std::filesystem::create_directories(directory);
		return directory / std::format("{}_{}.{}", prefix, makeTimestampSuffix(), extension);
	}

	std::optional<std::string> showOpenFileDialog(const char* title, const char* filter, const std::filesystem::path& initialDirectory)
	{
		std::filesystem::create_directories(initialDirectory);

		char fileName[MAX_PATH] = {};
		OPENFILENAMEA openFileName{};
		openFileName.lStructSize = sizeof(openFileName);
		openFileName.lpstrTitle = title;
		openFileName.lpstrFilter = filter;
		openFileName.lpstrFile = fileName;
		openFileName.nMaxFile = MAX_PATH;
		const std::string initialDirectoryString = initialDirectory.string();
		openFileName.lpstrInitialDir = initialDirectoryString.c_str();
		openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (!GetOpenFileNameA(&openFileName))
			return std::nullopt;

		return std::string(fileName);
	}

	pugi::xml_node writeVector3(pugi::xml_node parent, const char* name, Vector3 value)
	{
		pugi::xml_node node = parent.append_child(name);
		node.append_attribute("x") = value.x;
		node.append_attribute("y") = value.y;
		node.append_attribute("z") = value.z;
		return node;
	}

	Vector3 readVector3(pugi::xml_node parent, const char* name, Vector3 fallback = {})
	{
		pugi::xml_node node = parent.child(name);
		if (!node)
			return fallback;

		return { node.attribute("x").as_float(fallback.x), node.attribute("y").as_float(fallback.y), node.attribute("z").as_float(fallback.z) };
	}

	void addLayoutElement(ModelInfo& info, DXGI_FORMAT format, const char* semantic)
	{
		VertexLayoutElement element{};
		element.format = format;
		strcpy_s(element.semantic, semantic);
		info.layout.push_back(element);
	}
}

void SplineRoadPersistence::saveSpline(const SplineRoadTool& tool) const
{
	if (!tool.getRoad())
		return;

	const std::filesystem::path path = makeTimestampedExportPath("spline", "spline");
	saveSplineToFile(tool, path.string());
}

void SplineRoadPersistence::loadSpline(SplineRoadTool& tool) const
{
	const auto selectedPath = showOpenFileDialog("Load spline", "Spline files\0*.spline\0All files\0*.*\0", defaultSplineExportDirectory());
	if (selectedPath)
		loadSplineFromFile(tool, *selectedPath);
}

void SplineRoadPersistence::bakeAndSaveModel(SplineRoadTool& tool) const
{
	SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return;

	if (!tool.hasPreview() || tool.isPreviewDirty())
		tool.buildPreview();
	construction = tool.getRoad();
	if (!construction || construction->mesh.empty())
		return;

	const std::filesystem::path path = makeTimestampedExportPath("spline_model", "model");
	if (saveCurrentMeshModel(tool, path.string()))
		tool.bakePreview();
}

bool SplineRoadPersistence::saveSplineToFile(const SplineRoadTool& tool, const std::string& path) const
{
	const SplineConstruction* construction = tool.getRoad();
	if (!construction)
		return false;

	std::filesystem::create_directories(std::filesystem::path(path).parent_path());

	pugi::xml_document doc;
	pugi::xml_node declaration = doc.append_child(pugi::node_declaration);
	declaration.append_attribute("version") = "1.0";
	declaration.append_attribute("encoding") = "utf-8";

	pugi::xml_node root = doc.append_child("SplineRoad");
	root.append_attribute("version") = 1;

	pugi::xml_node editor = root.append_child("Editor");
	editor.append_attribute("autoBuildPreview") = tool.autoBuildPreview;
	editor.append_attribute("previewPhysics") = tool.previewPhysics;
	editor.append_attribute("addPointDistance") = tool.addPointDistance;

	pugi::xml_node profile = root.append_child("Profile");
	profile.append_attribute("type") = profileToString(tool.previewProfile);
	profile.append_attribute("width") = tool.profileWidth;
	profile.append_attribute("widthSegments") = tool.roadWidthSegments;
	profile.append_attribute("height") = tool.profileHeight;
	profile.append_attribute("radius") = tool.profileRadius;
	profile.append_attribute("innerRadius") = tool.profileInnerRadius;
	profile.append_attribute("arcAngleDegrees") = tool.profileArcAngleDegrees;
	profile.append_attribute("segments") = tool.profileSegments;

	pugi::xml_node sweep = root.append_child("Sweep");
	sweep.append_attribute("pathUvScale") = construction->sweepSettings.pathUvScale;
	sweep.append_attribute("profileUvScale") = construction->sweepSettings.profileUvScale;
	sweep.append_attribute("preventProfileFoldover") = tool.preventProfileFoldover;
	sweep.append_attribute("minProfileForwardScale") = tool.minProfileForwardScale;
	sweep.append_attribute("splitOpenProfileQuadsAtCenter") = construction->sweepSettings.splitOpenProfileQuadsAtCenter;
	sweep.append_attribute("splitClosedProfileQuadsAtCenter") = construction->sweepSettings.splitClosedProfileQuadsAtCenter;

	pugi::xml_node sampling = root.append_child("Sampling");
	sampling.append_attribute("tessellationSegments") = static_cast<unsigned int>(construction->spline.getTessellationSegments());
	sampling.append_attribute("maxTessellationSegments") = tool.maxTessellationSegments;
	sampling.append_attribute("curvatureTessellationScale") = tool.curvatureTessellationScale;
	sampling.append_attribute("rollTessellationScale") = tool.rollTessellationScale;

	pugi::xml_node spline = root.append_child("Spline");
	spline.append_attribute("closed") = construction->spline.isClosed();
	writeVector3(spline, "ReferenceUp", construction->spline.getReferenceUp());
	for (const SplinePoint& point : construction->spline.getPoints())
	{
		pugi::xml_node pointNode = spline.append_child("Point");
		pointNode.append_attribute("roll") = point.roll;
		pointNode.append_attribute("upStabilization") = point.upStabilization;
		writeVector3(pointNode, "Position", point.position);
	}

	if (!doc.save_file(path.c_str(), PUGIXML_TEXT("\t")))
	{
		Logger::logError("Failed to save spline " + path);
		return false;
	}

	Logger::log("Saved spline " + path);
	return true;
}

bool SplineRoadPersistence::loadSplineFromFile(SplineRoadTool& tool, const std::string& path) const
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(path.c_str());
	if (!result)
	{
		Logger::logError("Failed to load spline " + path);
		return false;
	}

	pugi::xml_node root = doc.child("SplineRoad");
	if (!root)
	{
		Logger::logError("Invalid spline file " + path);
		return false;
	}

	SplineRoadTool::SplineRoadInstance& instance = tool.createSplineInstance();
	SplineConstruction* construction = instance.construction.get();

	pugi::xml_node editor = root.child("Editor");
	tool.autoBuildPreview = editor.attribute("autoBuildPreview").as_bool(tool.autoBuildPreview);
	tool.previewPhysics = editor.attribute("previewPhysics").as_bool(tool.previewPhysics);
	tool.addPointDistance = editor.attribute("addPointDistance").as_float(tool.addPointDistance);

	pugi::xml_node profile = root.child("Profile");
	tool.previewProfile = profileFromString(profile.attribute("type").as_string(profileToString(tool.previewProfile)));
	tool.profileWidth = profile.attribute("width").as_float(tool.profileWidth);
	tool.roadWidthSegments = profile.attribute("widthSegments").as_int(tool.roadWidthSegments);
	tool.profileHeight = profile.attribute("height").as_float(tool.profileHeight);
	tool.profileRadius = profile.attribute("radius").as_float(tool.profileRadius);
	tool.profileInnerRadius = profile.attribute("innerRadius").as_float(tool.profileInnerRadius);
	tool.profileArcAngleDegrees = profile.attribute("arcAngleDegrees").as_float(tool.profileArcAngleDegrees);
	tool.profileSegments = profile.attribute("segments").as_int(tool.profileSegments);

	pugi::xml_node sweep = root.child("Sweep");
	construction->sweepSettings.pathUvScale = sweep.attribute("pathUvScale").as_float(0.25f);
	construction->sweepSettings.profileUvScale = sweep.attribute("profileUvScale").as_float(1.0f);
	tool.preventProfileFoldover = sweep.attribute("preventProfileFoldover").as_bool(tool.preventProfileFoldover);
	tool.minProfileForwardScale = sweep.attribute("minProfileForwardScale").as_float(tool.minProfileForwardScale);
	construction->sweepSettings.preventProfileFoldover = tool.preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = tool.minProfileForwardScale;
	construction->sweepSettings.splitOpenProfileQuadsAtCenter = sweep.attribute("splitOpenProfileQuadsAtCenter").as_bool(construction->sweepSettings.splitOpenProfileQuadsAtCenter);
	construction->sweepSettings.splitClosedProfileQuadsAtCenter = sweep.attribute("splitClosedProfileQuadsAtCenter").as_bool(construction->sweepSettings.splitClosedProfileQuadsAtCenter);

	pugi::xml_node sampling = root.child("Sampling");
	construction->spline.setTessellationSegments(sampling.attribute("tessellationSegments").as_uint(static_cast<unsigned int>(construction->spline.getTessellationSegments())));
	tool.maxTessellationSegments = sampling.attribute("maxTessellationSegments").as_int(tool.maxTessellationSegments);
	tool.curvatureTessellationScale = sampling.attribute("curvatureTessellationScale").as_float(tool.curvatureTessellationScale);
	tool.rollTessellationScale = sampling.attribute("rollTessellationScale").as_float(tool.rollTessellationScale);
	tool.applySplineSamplingSettings();

	pugi::xml_node spline = root.child("Spline");
	construction->spline.setClosed(spline.attribute("closed").as_bool(false));
	construction->spline.setReferenceUp(readVector3(spline, "ReferenceUp", Vector3::UnitY));
	for (pugi::xml_node pointNode : spline.children("Point"))
	{
		const Vector3 position = readVector3(pointNode, "Position");
		const float roll = pointNode.attribute("roll").as_float(0.0f);
		const float upStabilization = pointNode.attribute("upStabilization").as_float(1.0f);
		construction->spline.addPoint(position, roll);
		construction->spline.setPointUpStabilization(construction->spline.getPointCount() - 1, upStabilization);
	}

	construction->profile = tool.createPreviewProfile();
	tool.selectedPointIndex = construction->spline.getPointCount() ? construction->spline.getPointCount() - 1 : Spline::InvalidPointIndex;
	instance.previewBuilt = false;
	instance.previewDirty = false;

	if (tool.autoBuildPreview)
		tool.buildPreview();
	else
		tool.rebuildDebug();

	Logger::log("Loaded spline " + path);
	return true;
}

bool SplineRoadPersistence::saveCurrentMeshModel(const SplineRoadTool& tool, const std::string& path) const
{
	const SplineConstruction* construction = tool.getRoad();
	if (!construction || construction->mesh.empty())
		return false;

	if (construction->mesh.vertices.size() > (std::numeric_limits<uint16_t>::max)())
	{
		Logger::logError("Cannot save spline model with more than 65535 vertices");
		return false;
	}

	std::filesystem::create_directories(std::filesystem::path(path).parent_path());

	ModelInfo info;
	addLayoutElement(info, DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::POSITION);
	addLayoutElement(info, DXGI_FORMAT_R32G32_FLOAT, VertexElementSemantic::TEXCOORD);
	addLayoutElement(info, DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::NORMAL);
	addLayoutElement(info, DXGI_FORMAT_R32G32B32_FLOAT, VertexElementSemantic::TANGENT);

	info.vertexCount = static_cast<uint32_t>(construction->mesh.vertices.size());
	const char* vertexData = reinterpret_cast<const char*>(construction->mesh.vertices.data());
	info.vertexData.assign(vertexData, vertexData + construction->mesh.vertices.size() * sizeof(SplineSweepVertex));

	info.indexCount = static_cast<uint32_t>(construction->mesh.indices.size());
	info.indices.reserve(construction->mesh.indices.size());
	for (uint32_t index : construction->mesh.indices)
	{
		if (index > (std::numeric_limits<uint16_t>::max)())
		{
			Logger::logError("Cannot save spline model with index larger than 65535");
			return false;
		}
		info.indices.push_back(static_cast<uint16_t>(index));
	}

	if (!BinaryModelSerialization::SaveModel(path, info))
	{
		Logger::logError("Failed to save spline model " + path);
		return false;
	}

	Logger::log("Saved spline model " + path);
	return true;
}