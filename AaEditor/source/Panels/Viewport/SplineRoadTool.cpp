#include "SplineRoadTool.h"

#include "ApplicationCore.h"
#include "ViewportPanel.h"
#include "App/Directories.h"
#include "Resources/Model/BinaryModelSerialization.h"
#include "Scene/Camera.h"
#include "Scene/RenderEntity.h"
#include "Utils/Logger.h"
#include "../data/editor/fonts/IconsFontAwesome7.h"
#include "ImGuizmo.h"
#include "imgui.h"

#define PUGIXML_HEADER_ONLY
#include "pugixml.hpp"

#include <ResourceUploadBatch.h>
#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <format>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;

	float degreesToRadians(float degrees)
	{
		return degrees * Pi / 180.0f;
	}

	float radiansToDegrees(float radians)
	{
		return radians * 180.0f / Pi;
	}

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

	std::optional<std::string> showSaveFileDialog(const char* title, const char* filter, const char* defaultExtension, const std::filesystem::path& initialPath)
	{
		std::filesystem::create_directories(initialPath.parent_path());

		char fileName[MAX_PATH] = {};
		strcpy_s(fileName, initialPath.string().c_str());

		OPENFILENAMEA saveFileName{};
		saveFileName.lStructSize = sizeof(saveFileName);
		saveFileName.lpstrTitle = title;
		saveFileName.lpstrFilter = filter;
		saveFileName.lpstrFile = fileName;
		saveFileName.nMaxFile = MAX_PATH;
		saveFileName.lpstrDefExt = defaultExtension;
		saveFileName.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		if (!GetSaveFileNameA(&saveFileName))
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

SplineRoadTool::SplineRoadTool(ApplicationCore& a, ViewportPanel& v)
	: app(a), viewport(v)
{
	viewport.registerSplineRoadTool(this);
	updateDebugSettings();
}

SplineRoadTool::~SplineRoadTool()
{
	for (auto& spline : splines)
		removeSplineFromWorld(*spline);
	splines.clear();
}

void SplineRoadTool::onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera)
{
	if (pickInfo.id && trySelectKnownSplinePart(pickInfo.id, pickInfo.position))
		return;

	if (!ctrlActive && pickInfo.id)
		Logger::log("Spline road tool ignored non-spline entity pick");
}

void SplineRoadTool::draw(const ViewportToolContext& ctx)
{
	auto activeBg = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	auto inactiveBg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];

	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

	if (ImGui::Button(ICON_FA_ROUTE " ##spline_create", ImVec2(40, 40)))
		createRoad();

	ImGui::PushStyleColor(ImGuiCol_Button, hasRoad() ? inactiveBg : ImVec4(inactiveBg.x, inactiveBg.y, inactiveBg.z, 0.45f));
	if (ImGui::Button(ICON_FA_CIRCLE_PLUS " ##spline_add_point", ImVec2(40, 40)) && hasRoad())
		addPointForward();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, selectedPointIndex != Spline::InvalidPointIndex ? inactiveBg : ImVec4(inactiveBg.x, inactiveBg.y, inactiveBg.z, 0.45f));
	if (ImGui::Button(ICON_FA_CIRCLE_MINUS " ##spline_remove_point", ImVec2(40, 40)) && selectedPointIndex != Spline::InvalidPointIndex)
		removeSelectedPoint();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, hasPreview() ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_HAMMER " ##spline_build_preview", ImVec2(40, 40)) && hasRoad())
		buildPreview();
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::Move ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " ##spline_move_point", ImVec2(40, 40)))
		pointGizmoMode = PointGizmoMode::Move;
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Button, pointGizmoMode == PointGizmoMode::Roll ? activeBg : inactiveBg);
	if (ImGui::Button(ICON_FA_ARROWS_ROTATE " ##spline_roll_point", ImVec2(40, 40)))
		pointGizmoMode = PointGizmoMode::Roll;
	ImGui::PopStyleColor();

	ImGui::PopStyleVar();

	ImGui::SetNextItemWidth(90);
	ImGui::DragFloat("##SplinePointDistance", &addPointDistance, 0.5f, 1.0f, 100.0f, "%.1f m");

	ImGui::EndGroup();
	overlayActive = ImGui::IsItemHovered();

	drawSelectedPointGizmo(ctx);
}

void SplineRoadTool::onActivated()
{
	editModeActive = true;
	rebuildAllDebug();
}

void SplineRoadTool::onDeactivated()
{
	editModeActive = false;
	gizmoActive = false;
	rebuildAllDebug();
}

void SplineRoadTool::reset()
{
	overlayActive = false;
	gizmoActive = false;
}

void SplineRoadTool::createRoad()
{
	SplineRoadInstance& instance = createSplineInstance();
	SplineConstruction* construction = instance.construction.get();
	construction->spline.addPoint({ -12.0f, 1.0f, 0.0f });
	construction->spline.addPoint({ 0.0f, 1.0f, 12.0f });
	construction->spline.addPoint({ 12.0f, 1.0f, 0.0f });
	applySplineSamplingSettings();
	construction->profile = createPreviewProfile();
	construction->sweepSettings.pathUvScale = 0.25f;
	construction->sweepSettings.preventProfileFoldover = preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = minProfileForwardScale;
	selectedPointIndex = construction->spline.getPointCount() - 1;
	instance.previewDirty = false;
	rebuildDebug();
}

void SplineRoadTool::clearRoad()
{
	SplineRoadInstance* instance = activeSpline();
	if (!instance)
		return;

	removeSplineFromWorld(*instance);
	splines.erase(splines.begin() + activeSplineIndex);
	if (splines.empty())
		activeSplineIndex = Spline::InvalidPointIndex;
	else
		activeSplineIndex = (std::min)(activeSplineIndex, splines.size() - 1);
	selectedPointIndex = Spline::InvalidPointIndex;
	rebuildAllDebug();
}

bool SplineRoadTool::hasPreview() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance && instance->previewBuilt;
}

bool SplineRoadTool::isPreviewDirty() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance && instance->previewDirty;
}

SplineConstruction* SplineRoadTool::getRoad()
{
	SplineRoadInstance* instance = activeSpline();
	return instance ? instance->construction.get() : nullptr;
}

const SplineConstruction* SplineRoadTool::getRoad() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance ? instance->construction.get() : nullptr;
}

uint32_t SplineRoadTool::getActiveSplineId() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance ? instance->id : 0;
}

const char* SplineRoadTool::getActiveSplineName() const
{
	const SplineRoadInstance* instance = activeSpline();
	return instance ? instance->name.c_str() : "None";
}

const char* SplineRoadTool::getSplineName(size_t index) const
{
	return index < splines.size() ? splines[index]->name.c_str() : "";
}

void SplineRoadTool::selectSplineByIndex(size_t index, bool movementMode)
{
	if (index >= splines.size())
		return;

	activeSplineIndex = index;
	selectedPointIndex = Spline::InvalidPointIndex;
	if (movementMode)
		enterSplineTransformMode();
	else
		rebuildAllDebug();
}

void SplineRoadTool::enterPointEditMode()
{
	if (pointGizmoMode == PointGizmoMode::SplineTransform)
		pointGizmoMode = PointGizmoMode::Move;
	rebuildAllDebug();
}

void SplineRoadTool::enterSplineTransformMode()
{
	selectedPointIndex = Spline::InvalidPointIndex;
	pointGizmoMode = PointGizmoMode::SplineTransform;
	rebuildAllDebug();
}

void SplineRoadTool::selectPreviousPoint()
{
	selectPointByOffset(-1);
}

void SplineRoadTool::selectNextPoint()
{
	selectPointByOffset(1);
}

SplineRoadTool::SplineRoadInstance* SplineRoadTool::activeSpline()
{
	return activeSplineIndex < splines.size() ? splines[activeSplineIndex].get() : nullptr;
}

const SplineRoadTool::SplineRoadInstance* SplineRoadTool::activeSpline() const
{
	return activeSplineIndex < splines.size() ? splines[activeSplineIndex].get() : nullptr;
}

SplineRoadTool::SplineRoadInstance* SplineRoadTool::findSplineByPathEntity(RenderEntity* entity)
{
	for (auto& spline : splines)
		if (spline->debugVisualization.getPathEntity() == entity)
			return spline.get();

	return nullptr;
}

SplineRoadTool::SplineRoadInstance& SplineRoadTool::createSplineInstance()
{
	auto instance = std::make_unique<SplineRoadInstance>();
	instance->id = nextSplineId++;
	instance->name = std::format("Spline {}", instance->id);
	instance->construction = std::make_unique<SplineConstruction>();
	splines.push_back(std::move(instance));
	activeSplineIndex = splines.size() - 1;
	selectedPointIndex = Spline::InvalidPointIndex;
	return *splines.back();
}

void SplineRoadTool::removeSplineFromWorld(SplineRoadInstance& instance)
{
	if (instance.construction)
		instance.construction->removeFromWorld(app.renderWorld, &app.physicsMgr);
	instance.debugVisualization.removeFromWorld(app.renderWorld);
	instance.previewBuilt = false;
	instance.previewDirty = false;
}

void SplineRoadTool::addPointForward()
{
	ensureRoad();
	SplineConstruction* construction = getRoad();
	auto& spline = construction->spline;
	const size_t pointCount = spline.getPointCount();

	if (pointCount == 0)
	{
		spline.addPoint({});
		selectedPointIndex = 0;
		markConstructionChanged();
		return;
	}

	switch (addPointMode)
	{
	case AddPointMode::InsertAfter:
		if (selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex + 1 < pointCount)
		{
			insertPointBetween(selectedPointIndex, selectedPointIndex + 1);
			return;
		}
		break;
	case AddPointMode::InsertBefore:
		if (selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex > 0 && selectedPointIndex < pointCount)
		{
			insertPointBetween(selectedPointIndex - 1, selectedPointIndex);
			return;
		}
		break;
	case AddPointMode::ExtendStart:
		selectedPointIndex = 0;
		break;
	case AddPointMode::ExtendEnd:
		selectedPointIndex = pointCount - 1;
		break;
	case AddPointMode::Auto:
	default:
		if (selectedPointIndex > 0 && selectedPointIndex != Spline::InvalidPointIndex && selectedPointIndex + 1 < pointCount)
		{
			insertPointBetween(selectedPointIndex, selectedPointIndex + 1);
			return;
		}
		break;
	}

	bool extendStart = shouldExtendStart(pointCount);
	if (replicateCurveOnAdd && addReplicatedCurvePoint(extendStart))
	{
		markConstructionChanged();
		return;
	}

	Vector3 direction = getForwardAddDirection(extendStart);
	if (extendStart)
	{
		Vector3 position = spline.getPoint(0).position + direction * addPointDistance;
		spline.insertPoint(0, position);
		selectedPointIndex = 0;
	}
	else
	{
		Vector3 position = spline.getPoint(pointCount - 1).position + direction * addPointDistance;
		spline.addPoint(position);
		selectedPointIndex = spline.getPointCount() - 1;
	}

	markConstructionChanged();
}

void SplineRoadTool::removeSelectedPoint()
{
	SplineConstruction* construction = getRoad();
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.removePoint(selectedPointIndex);
	const size_t pointCount = construction->spline.getPointCount();
	selectedPointIndex = pointCount ? (std::min)(selectedPointIndex, pointCount - 1) : Spline::InvalidPointIndex;
	markConstructionChanged();
}

void SplineRoadTool::buildPreview()
{
	SplineRoadInstance* instance = activeSpline();
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	applySplineSamplingSettings();
	construction->profile = createPreviewProfile();
	construction->sweepSettings.preventProfileFoldover = preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = minProfileForwardScale;

	DirectX::ResourceUploadBatch batch(app.renderSystem.core.device);
	batch.Begin();

	SplineConstructionCreateParams params;
	params.materialName = "General";
	params.createMeshPhysics = previewPhysics;
	construction->regenerate(app.renderSystem, app.resources, app.renderWorld, &app.physicsMgr, batch, params);

	auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	instance->previewBuilt = construction->entity != nullptr;
	instance->previewDirty = false;
	rebuildDebug();
}

void SplineRoadTool::bakePreview()
{
	SplineRoadInstance* instance = activeSpline();
	SplineConstruction* construction = getRoad();
	if (!construction || !construction->entity)
		return;

	instance->debugVisualization.removeFromWorld(app.renderWorld);
	bakedConstructions.push_back(std::move(instance->construction));
	splines.erase(splines.begin() + activeSplineIndex);
	if (splines.empty())
		activeSplineIndex = Spline::InvalidPointIndex;
	else
		activeSplineIndex = (std::min)(activeSplineIndex, splines.size() - 1);
	selectedPointIndex = Spline::InvalidPointIndex;
	rebuildAllDebug();
	Logger::log("Spline road preview baked into a regular render entity");
}

void SplineRoadTool::copyActiveSpline()
{
	const SplineRoadInstance* source = activeSpline();
	if (!source || !source->construction)
		return;

	SplineRoadInstance& copy = createSplineInstance();
	copy.name = std::format("Spline {} copy", source->id);
	copy.construction->spline = source->construction->spline;
	copy.construction->profile = source->construction->profile;
	copy.construction->sweepSettings = source->construction->sweepSettings;
	copy.previewDirty = true;
	selectedPointIndex = copy.construction->spline.getPointCount() ? copy.construction->spline.getPointCount() - 1 : Spline::InvalidPointIndex;
	if (autoBuildPreview)
		buildPreview();
	else
		rebuildAllDebug();
}

void SplineRoadTool::saveSpline()
{
	if (!getRoad())
		return;

	const std::filesystem::path path = makeTimestampedExportPath("spline", "spline");

	// To save with a Windows dialog instead of the default timestamped path:
	// const auto selectedPath = showSaveFileDialog("Save spline", "Spline files\0*.spline\0All files\0*.*\0", "spline", path);
	// if (!selectedPath)
	// 	return;
	// saveSplineToFile(*selectedPath);
	// return;

	saveSplineToFile(path.string());
}

void SplineRoadTool::loadSpline()
{
	const auto selectedPath = showOpenFileDialog("Load spline", "Spline files\0*.spline\0All files\0*.*\0", defaultSplineExportDirectory());
	if (!selectedPath)
		return;

	loadSplineFromFile(*selectedPath);
}

void SplineRoadTool::bakeAndSaveModel()
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	if (!hasPreview() || isPreviewDirty())
		buildPreview();
	construction = getRoad();
	if (!construction || construction->mesh.empty())
		return;

	const std::filesystem::path path = makeTimestampedExportPath("spline_model", "model");

	// To save with a Windows dialog instead of the default timestamped path:
	// const auto selectedPath = showSaveFileDialog("Save baked model", "Model files\0*.model\0All files\0*.*\0", "model", path);
	// if (!selectedPath)
	// 	return;
	// if (saveCurrentMeshModel(*selectedPath))
	// 	bakePreview();
	// return;

	if (saveCurrentMeshModel(path.string()))
		bakePreview();
}

bool SplineRoadTool::saveSplineToFile(const std::string& path) const
{
	const SplineConstruction* construction = getRoad();
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
	editor.append_attribute("autoBuildPreview") = autoBuildPreview;
	editor.append_attribute("previewPhysics") = previewPhysics;
	editor.append_attribute("addPointDistance") = addPointDistance;

	pugi::xml_node profile = root.append_child("Profile");
	profile.append_attribute("type") = profileToString(previewProfile);
	profile.append_attribute("width") = profileWidth;
	profile.append_attribute("widthSegments") = roadWidthSegments;
	profile.append_attribute("height") = profileHeight;
	profile.append_attribute("radius") = profileRadius;
	profile.append_attribute("innerRadius") = profileInnerRadius;
	profile.append_attribute("arcAngleDegrees") = profileArcAngleDegrees;
	profile.append_attribute("segments") = profileSegments;

	pugi::xml_node sweep = root.append_child("Sweep");
	sweep.append_attribute("pathUvScale") = construction->sweepSettings.pathUvScale;
	sweep.append_attribute("profileUvScale") = construction->sweepSettings.profileUvScale;
	sweep.append_attribute("preventProfileFoldover") = preventProfileFoldover;
	sweep.append_attribute("minProfileForwardScale") = minProfileForwardScale;
	sweep.append_attribute("splitOpenProfileQuadsAtCenter") = construction->sweepSettings.splitOpenProfileQuadsAtCenter;
	sweep.append_attribute("splitClosedProfileQuadsAtCenter") = construction->sweepSettings.splitClosedProfileQuadsAtCenter;

	pugi::xml_node sampling = root.append_child("Sampling");
	sampling.append_attribute("tessellationSegments") = static_cast<unsigned int>(construction->spline.getTessellationSegments());
	sampling.append_attribute("maxTessellationSegments") = maxTessellationSegments;
	sampling.append_attribute("curvatureTessellationScale") = curvatureTessellationScale;
	sampling.append_attribute("rollTessellationScale") = rollTessellationScale;

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

bool SplineRoadTool::loadSplineFromFile(const std::string& path)
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

	SplineRoadInstance& instance = createSplineInstance();
	SplineConstruction* construction = instance.construction.get();

	pugi::xml_node editor = root.child("Editor");
	autoBuildPreview = editor.attribute("autoBuildPreview").as_bool(autoBuildPreview);
	previewPhysics = editor.attribute("previewPhysics").as_bool(previewPhysics);
	addPointDistance = editor.attribute("addPointDistance").as_float(addPointDistance);

	pugi::xml_node profile = root.child("Profile");
	previewProfile = profileFromString(profile.attribute("type").as_string(profileToString(previewProfile)));
	profileWidth = profile.attribute("width").as_float(profileWidth);
	roadWidthSegments = profile.attribute("widthSegments").as_int(roadWidthSegments);
	profileHeight = profile.attribute("height").as_float(profileHeight);
	profileRadius = profile.attribute("radius").as_float(profileRadius);
	profileInnerRadius = profile.attribute("innerRadius").as_float(profileInnerRadius);
	profileArcAngleDegrees = profile.attribute("arcAngleDegrees").as_float(profileArcAngleDegrees);
	profileSegments = profile.attribute("segments").as_int(profileSegments);

	pugi::xml_node sweep = root.child("Sweep");
	construction->sweepSettings.pathUvScale = sweep.attribute("pathUvScale").as_float(0.25f);
	construction->sweepSettings.profileUvScale = sweep.attribute("profileUvScale").as_float(1.0f);
	preventProfileFoldover = sweep.attribute("preventProfileFoldover").as_bool(preventProfileFoldover);
	minProfileForwardScale = sweep.attribute("minProfileForwardScale").as_float(minProfileForwardScale);
	construction->sweepSettings.preventProfileFoldover = preventProfileFoldover;
	construction->sweepSettings.minProfileForwardScale = minProfileForwardScale;
	construction->sweepSettings.splitOpenProfileQuadsAtCenter = sweep.attribute("splitOpenProfileQuadsAtCenter").as_bool(construction->sweepSettings.splitOpenProfileQuadsAtCenter);
	construction->sweepSettings.splitClosedProfileQuadsAtCenter = sweep.attribute("splitClosedProfileQuadsAtCenter").as_bool(construction->sweepSettings.splitClosedProfileQuadsAtCenter);

	pugi::xml_node sampling = root.child("Sampling");
	construction->spline.setTessellationSegments(sampling.attribute("tessellationSegments").as_uint(static_cast<unsigned int>(construction->spline.getTessellationSegments())));
	maxTessellationSegments = sampling.attribute("maxTessellationSegments").as_int(maxTessellationSegments);
	curvatureTessellationScale = sampling.attribute("curvatureTessellationScale").as_float(curvatureTessellationScale);
	rollTessellationScale = sampling.attribute("rollTessellationScale").as_float(rollTessellationScale);
	applySplineSamplingSettings();

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

	construction->profile = createPreviewProfile();
	selectedPointIndex = construction->spline.getPointCount() ? construction->spline.getPointCount() - 1 : Spline::InvalidPointIndex;
	instance.previewBuilt = false;
	instance.previewDirty = false;

	if (autoBuildPreview)
		buildPreview();
	else
		rebuildDebug();

	Logger::log("Loaded spline " + path);
	return true;
}

bool SplineRoadTool::saveCurrentMeshModel(const std::string& path) const
{
	const SplineConstruction* construction = getRoad();
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

void SplineRoadTool::setSelectedPointPosition(Vector3 position)
{
	SplineConstruction* construction = getRoad();
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPoint(selectedPointIndex, position);
	markConstructionChanged();
}

void SplineRoadTool::setSelectedPointRoll(float roll)
{
	SplineConstruction* construction = getRoad();
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPointRoll(selectedPointIndex, roll);
	markConstructionChanged();
}

void SplineRoadTool::setSelectedPointUpStabilization(float upStabilization)
{
	SplineConstruction* construction = getRoad();
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
		return;

	construction->spline.setPointUpStabilization(selectedPointIndex, upStabilization);
	markConstructionChanged();
}

void SplineRoadTool::onAutoBuildPreviewChanged()
{
	if (autoBuildPreview && getRoad() && (isPreviewDirty() || !hasPreview()))
		buildPreview();
}

void SplineRoadTool::onProfileSettingsChanged()
{
	markPreviewSettingsChanged();
}

void SplineRoadTool::onSplineSamplingSettingsChanged()
{
	if (!getRoad())
		return;

	applySplineSamplingSettings();
	markConstructionChanged();
}

void SplineRoadTool::refreshDebugVisualization()
{
	rebuildAllDebug();
}

bool SplineRoadTool::trySelectKnownSplinePart(ObjectId id, Vector3 pickedPosition)
{
	auto object = app.renderWorld.getObject(id);
	if (!object.entity)
		return false;

	SplineRoadInstance* pathInstance = findSplineByPathEntity(object.entity);
	if (pathInstance)
	{
		if (editModeActive)
			return true;

		for (size_t i = 0; i < splines.size(); ++i)
		{
			if (splines[i].get() == pathInstance)
			{
				activeSplineIndex = i;
				break;
			}
		}
		selectedPointIndex = Spline::InvalidPointIndex;
		enterSplineTransformMode();
		Logger::log(std::format("Selected {} for spline movement", pathInstance->name));
		return true;
	}

	SplineRoadInstance* instance = activeSpline();
	SplineConstruction* construction = getRoad();
	if (!instance || !construction)
		return false;

	size_t pointIndex = instance->debugVisualization.getPointIndex(object.entity);
	if (pointIndex != Spline::InvalidPointIndex)
	{
		selectedPointIndex = pointIndex;
		enterPointEditMode();
		if (autoBuildPreview && (instance->previewDirty || !instance->previewBuilt))
			buildPreview();
		Logger::log(std::format("Selected spline point {}", pointIndex));
		return true;
	}

	return false;
}

bool SplineRoadTool::selectPathForMovement(ObjectId id, Vector3 pickedPosition)
{
	return id && trySelectKnownSplinePart(id, pickedPosition) && pointGizmoMode == PointGizmoMode::SplineTransform;
}

void SplineRoadTool::rebuildDebug()
{
	SplineRoadInstance* instance = activeSpline();
	if (!instance)
		return;

	rebuildDebug(*instance);
}

void SplineRoadTool::rebuildDebug(SplineRoadInstance& instance)
{
	if (!instance.construction)
		return;

	updateDebugSettings();
	debugSettings.showPath = true;
	debugSettings.showPoints = editModeActive && &instance == activeSpline();
	debugSettings.showActivePointFrame = editModeActive && &instance == activeSpline() && showActiveFrame;
	debugSettings.activePointIndex = &instance == activeSpline() ? selectedPointIndex : Spline::InvalidPointIndex;

	DirectX::ResourceUploadBatch batch(app.renderSystem.core.device);
	batch.Begin();
	instance.debugVisualization.rebuild(instance.construction->spline, app.renderSystem, app.resources, app.renderWorld, batch, debugSettings);
	auto uploadResourcesFinished = batch.End(app.renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();
}

void SplineRoadTool::rebuildAllDebug()
{
	for (auto& instance : splines)
		rebuildDebug(*instance);
}

void SplineRoadTool::removePreview()
{
	SplineRoadInstance* instance = activeSpline();
	if (!instance)
		return;

	if (instance->construction)
		instance->construction->removeFromWorld(app.renderWorld, &app.physicsMgr);
	instance->previewBuilt = false;
	instance->previewDirty = false;
}

void SplineRoadTool::markConstructionChanged()
{
	SplineRoadInstance* instance = activeSpline();
	if (instance && instance->construction && instance->construction->entity)
		instance->previewDirty = true;
	else
		if (instance)
			instance->previewDirty = false;

	if (autoBuildPreview && getRoad())
		buildPreview();
	else
		rebuildAllDebug();
}

void SplineRoadTool::markPreviewSettingsChanged()
{
	SplineRoadInstance* instance = activeSpline();
	if (instance && instance->construction && instance->construction->entity)
		instance->previewDirty = true;

	if (autoBuildPreview && getRoad())
		buildPreview();
}

void SplineRoadTool::updateDebugSettings()
{
	debugSettings.showPath = showDebugPath;
	debugSettings.showPoints = showDebugPoints;
	debugSettings.showActivePointFrame = showActiveFrame;
	debugSettings.activePointIndex = selectedPointIndex;
}

void SplineRoadTool::applySplineSamplingSettings()
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	construction->spline.setAdaptiveTessellation(curvatureTessellationScale, static_cast<size_t>((std::max)(maxTessellationSegments, 1)), rollTessellationScale);
}

void SplineRoadTool::ensureRoad()
{
	if (!getRoad())
		createRoad();
}

ShapeProfile2D SplineRoadTool::createPreviewProfile() const
{
	const size_t segments = static_cast<size_t>((std::max)(profileSegments, 3));
	const float width = (std::max)(profileWidth, 0.1f);
	const float height = (std::max)(profileHeight, 0.1f);
	const float radius = (std::max)(profileRadius, 0.1f);
	const float innerRadius = (std::min)((std::max)(profileInnerRadius, 0.01f), radius * 0.95f);
	const float arcAngle = degreesToRadians((std::max)(profileArcAngleDegrees, 1.0f));

	switch (previewProfile)
	{
	case PreviewProfile::Rectangle:
		return ShapeProfile2D::createRectangle(width, height);
	case PreviewProfile::Circle:
		return ShapeProfile2D::createCircle(radius, segments);
	case PreviewProfile::Tube:
		return ShapeProfile2D::createTube(radius, innerRadius, segments);
	case PreviewProfile::Arc:
		return ShapeProfile2D::createArc(radius, arcAngle, segments);
	case PreviewProfile::FilledArc:
		return ShapeProfile2D::createFilledArc(radius, arcAngle, segments);
	case PreviewProfile::Road:
	default:
		return ShapeProfile2D::createRoad(width, static_cast<size_t>((std::max)(roadWidthSegments, 1)));
	}
}

void SplineRoadTool::drawSelectedPointGizmo(const ViewportToolContext& ctx)
{
	if (pointGizmoMode == PointGizmoMode::SplineTransform)
	{
		drawSplineTransformGizmo(ctx);
		return;
	}

	SplineConstruction* construction = getRoad();
	if (!construction || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= construction->spline.getPointCount())
	{
		gizmoActive = false;
		return;
	}

	if (pointGizmoMode == PointGizmoMode::Roll)
		drawSelectedPointRollGizmo(ctx);
	else
		drawSelectedPointMoveGizmo(ctx);
}

void SplineRoadTool::drawSelectedPointMoveGizmo(const ViewportToolContext& ctx)
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect((float)ctx.viewportBounds[0].x, (float)ctx.viewportBounds[0].y, (float)ctx.viewportBounds[1].x - ctx.viewportBounds[0].x, (float)ctx.viewportBounds[1].y - ctx.viewportBounds[0].y);

	XMFLOAT4X4 cameraView;
	XMStoreFloat4x4(&cameraView, ctx.camera.getViewMatrix());
	XMFLOAT4X4 cameraProjection;
	XMStoreFloat4x4(&cameraProjection, ctx.camera.getProjectionMatrixNoReverse());

	ObjectTransformation pointTransform;
	pointTransform.position = construction->spline.getPoint(selectedPointIndex).position;

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, pointTransform.createWorldMatrix());

	float snapValues[3] = { 0.5f, 0.5f, 0.5f };
	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &transform._11, nullptr, snapValues);
	gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

	if (!manipulated)
		return;

	ObjectTransformation updatedTransform;
	updatedTransform.fromWorldMatrix(XMLoadFloat4x4(&transform));

	const Vector3 oldPosition = construction->spline.getPoint(selectedPointIndex).position;
	if ((updatedTransform.position - oldPosition).LengthSquared() > 0.000001f)
		setSelectedPointPosition(updatedTransform.position);
}

void SplineRoadTool::drawSelectedPointRollGizmo(const ViewportToolContext& ctx)
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect((float)ctx.viewportBounds[0].x, (float)ctx.viewportBounds[0].y, (float)ctx.viewportBounds[1].x - ctx.viewportBounds[0].x, (float)ctx.viewportBounds[1].y - ctx.viewportBounds[0].y);

	XMFLOAT4X4 cameraView;
	XMStoreFloat4x4(&cameraView, ctx.camera.getViewMatrix());
	XMFLOAT4X4 cameraProjection;
	XMStoreFloat4x4(&cameraProjection, ctx.camera.getProjectionMatrixNoReverse());

	const SplinePoint point = construction->spline.getPoint(selectedPointIndex);
	SplineSample sample = construction->spline.evaluate(construction->spline.getSegmentCount() > 0 ? static_cast<float>(selectedPointIndex) / static_cast<float>(construction->spline.getSegmentCount()) : 0.0f);
	float closestDistanceSquared = (sample.position - point.position).LengthSquared();
	for (const SplineSample& candidate : construction->spline.getSamples())
	{
		const float distanceSquared = (candidate.position - point.position).LengthSquared();
		if (distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			sample = candidate;
		}
	}
	ObjectTransformation pointTransform;
	pointTransform.position = point.position;
	pointTransform.orientation = Quaternion::CreateFromRotationMatrix(Matrix::CreateWorld(Vector3::Zero, sample.tangent, sample.up));

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, pointTransform.createWorldMatrix());
	XMFLOAT4X4 transformDelta{};

	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, ImGuizmo::ROTATE_Z, ImGuizmo::LOCAL, &transform._11, &transformDelta._11, nullptr);
	gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

	if (!manipulated)
		return;

	XMVECTOR scale;
	XMVECTOR rotation;
	XMVECTOR translation;
	if (!XMMatrixDecompose(&scale, &rotation, &translation, XMLoadFloat4x4(&transformDelta)))
		return;

	Quaternion deltaRotation;
	XMStoreFloat4(&deltaRotation, rotation);
	Vector3 axis(deltaRotation.x, deltaRotation.y, deltaRotation.z);
	const float axisLength = axis.Length();
	if (axisLength <= 0.000001f)
		return;

	axis /= axisLength;
	float deltaAngle = 2.0f * std::atan2(axisLength, deltaRotation.w);
	if (deltaAngle > Pi)
		deltaAngle -= Pi * 2.0f;
	if (deltaAngle < -Pi)
		deltaAngle += Pi * 2.0f;

	const float rollDelta = deltaAngle * axis.Dot(sample.tangent);
	if (std::abs(rollDelta) > 0.00001f)
		setSelectedPointRoll(point.roll + rollDelta);
}

void SplineRoadTool::drawSplineTransformGizmo(const ViewportToolContext& ctx)
{
	SplineConstruction* construction = getRoad();
	if (!construction || construction->spline.getPointCount() == 0)
	{
		gizmoActive = false;
		return;
	}

	Vector3 center{};
	for (const SplinePoint& point : construction->spline.getPoints())
		center += point.position;
	center /= static_cast<float>(construction->spline.getPointCount());

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect((float)ctx.viewportBounds[0].x, (float)ctx.viewportBounds[0].y, (float)ctx.viewportBounds[1].x - ctx.viewportBounds[0].x, (float)ctx.viewportBounds[1].y - ctx.viewportBounds[0].y);

	XMFLOAT4X4 cameraView;
	XMStoreFloat4x4(&cameraView, ctx.camera.getViewMatrix());
	XMFLOAT4X4 cameraProjection;
	XMStoreFloat4x4(&cameraProjection, ctx.camera.getProjectionMatrixNoReverse());

	ObjectTransformation splineTransform;
	splineTransform.position = center;

	XMFLOAT4X4 transform;
	XMStoreFloat4x4(&transform, splineTransform.createWorldMatrix());
	XMFLOAT4X4 transformDelta{};

	float snapValues[3] = { 0.5f, 0.5f, 0.5f };
	const ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE | ImGuizmo::ROTATE);
	const bool manipulated = ImGuizmo::Manipulate(&cameraView._11, &cameraProjection._11, operation, ImGuizmo::WORLD, &transform._11, &transformDelta._11, snapValues);
	gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
	if (!manipulated)
		return;

	XMVECTOR scale;
	XMVECTOR rotation;
	XMVECTOR translation;
	if (!XMMatrixDecompose(&scale, &rotation, &translation, XMLoadFloat4x4(&transformDelta)))
		return;

	Quaternion deltaRotation;
	XMStoreFloat4(&deltaRotation, rotation);
	Vector3 deltaTranslation;
	XMStoreFloat3(&deltaTranslation, translation);

	for (size_t i = 0; i < construction->spline.getPointCount(); ++i)
	{
		SplinePoint point = construction->spline.getPoint(i);
		point.position = center + deltaRotation * (point.position - center) + deltaTranslation;
		construction->spline.setPoint(i, point);
	}

	Vector3 referenceUp = deltaRotation * construction->spline.getReferenceUp();
	construction->spline.setReferenceUp(referenceUp);
	markConstructionChanged();
}

void SplineRoadTool::insertPointBetween(size_t previousPointIndex, size_t nextPointIndex)
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	auto& spline = construction->spline;
	const auto& previousPoint = spline.getPoint(previousPointIndex);
	const auto& nextPoint = spline.getPoint(nextPointIndex);
	SplinePoint insertedPoint;
	insertedPoint.position = (previousPoint.position + nextPoint.position) * 0.5f;
	insertedPoint.roll = (previousPoint.roll + nextPoint.roll) * 0.5f;
	insertedPoint.upStabilization = (previousPoint.upStabilization + nextPoint.upStabilization) * 0.5f;
	spline.insertPoint(nextPointIndex, insertedPoint.position, insertedPoint.roll);
	spline.setPoint(nextPointIndex, insertedPoint);
	selectedPointIndex = nextPointIndex;
	markConstructionChanged();
}

void SplineRoadTool::selectPointByOffset(int offset)
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return;

	const size_t pointCount = construction->spline.getPointCount();
	if (pointCount == 0 || selectedPointIndex == Spline::InvalidPointIndex || selectedPointIndex >= pointCount)
		return;

	const int currentIndex = static_cast<int>(selectedPointIndex);
	const int count = static_cast<int>(pointCount);
	selectedPointIndex = static_cast<size_t>((currentIndex + offset + count) % count);
	enterPointEditMode();
	Logger::log(std::format("Selected spline point {}", selectedPointIndex));
}

bool SplineRoadTool::addReplicatedCurvePoint(bool extendStart)
{
	SplineConstruction* construction = getRoad();
	if (!construction)
		return false;

	auto& spline = construction->spline;
	const size_t pointCount = spline.getPointCount();
	if (pointCount < 3)
		return false;

	const size_t i0 = extendStart ? 2 : pointCount - 3;
	const size_t i1 = extendStart ? 1 : pointCount - 2;
	const size_t i2 = extendStart ? 0 : pointCount - 1;
	const SplinePoint p0 = spline.getPoint(i0);
	const SplinePoint p1 = spline.getPoint(i1);
	const SplinePoint p2 = spline.getPoint(i2);

	const Vector3 previousOffset = p1.position - p0.position;
	const Vector3 currentOffset = p2.position - p1.position;
	if (previousOffset.LengthSquared() <= 0.000001f || currentOffset.LengthSquared() <= 0.000001f)
		return false;

	Vector3 offset = currentOffset;
	Vector3 previousHorizontal = previousOffset;
	previousHorizontal.y = 0.0f;
	Vector3 currentHorizontal = currentOffset;
	currentHorizontal.y = 0.0f;
	if (replicateCurveMode == ReplicateCurveMode::Loop)
	{
		Vector3 forward = previousHorizontal + currentHorizontal;
		if (forward.LengthSquared() <= 0.000001f)
			forward = currentHorizontal.LengthSquared() > 0.000001f ? currentHorizontal : previousHorizontal;

		if (forward.LengthSquared() > 0.000001f)
		{
			forward.Normalize();
			Vector3 lateral = Vector3::UnitY.Cross(forward);
			lateral.Normalize();

			Vector2 previousPlane(previousOffset.Dot(forward), previousOffset.y);
			Vector2 currentPlane(currentOffset.Dot(forward), currentOffset.y);
			if (previousPlane.LengthSquared() > 0.000001f && currentPlane.LengthSquared() > 0.000001f)
			{
				previousPlane.Normalize();
				currentPlane.Normalize();
				const float signedPitch = std::atan2(previousPlane.x * currentPlane.y - previousPlane.y * currentPlane.x, std::clamp(previousPlane.Dot(currentPlane), -1.0f, 1.0f));

				const float cosPitch = std::cos(signedPitch);
				const float sinPitch = std::sin(signedPitch);
				const float currentForward = currentOffset.Dot(forward);
				const float currentVertical = currentOffset.y;
				const float nextForward = currentForward * cosPitch - currentVertical * sinPitch;
				const float nextVertical = currentForward * sinPitch + currentVertical * cosPitch;
				const float lateralDrift = currentOffset.Dot(lateral);
				offset = forward * nextForward + Vector3::UnitY * nextVertical + lateral * lateralDrift;
			}
		}
	}
	else if (previousHorizontal.LengthSquared() > 0.000001f && currentHorizontal.LengthSquared() > 0.000001f)
	{
		previousHorizontal.Normalize();
		currentHorizontal.Normalize();
		const float signedYaw = std::atan2(previousHorizontal.Cross(currentHorizontal).Dot(Vector3::UnitY), std::clamp(previousHorizontal.Dot(currentHorizontal), -1.0f, 1.0f));
		offset = Quaternion::CreateFromAxisAngle(Vector3::UnitY, signedYaw) * currentOffset;
	}

	SplinePoint newPoint = p2;
	newPoint.position = p2.position + offset;
	newPoint.roll = p2.roll + (p2.roll - p1.roll);
	newPoint.upStabilization = std::clamp(p2.upStabilization + (p2.upStabilization - p1.upStabilization), 0.0f, 1.0f);

	if (extendStart)
	{
		newPoint.position = p2.position + offset;
		spline.insertPoint(0, newPoint.position, newPoint.roll);
		spline.setPoint(0, newPoint);
		selectedPointIndex = 0;
	}
	else
	{
		spline.addPoint(newPoint.position, newPoint.roll);
		spline.setPoint(spline.getPointCount() - 1, newPoint);
		selectedPointIndex = spline.getPointCount() - 1;
	}

	return true;
}

bool SplineRoadTool::shouldExtendStart(size_t pointCount) const
{
	if (addPointMode == AddPointMode::ExtendStart)
		return true;
	if (addPointMode == AddPointMode::ExtendEnd)
		return false;

	return selectedPointIndex == 0 && pointCount > 1;
}

Vector3 SplineRoadTool::getForwardAddDirection(bool extendStart) const
{
	const SplineConstruction* construction = getRoad();
	if (!construction || construction->spline.getPointCount() < 2)
		return Vector3::UnitZ;

	const auto& spline = construction->spline;
	Vector3 direction;
	if (extendStart)
		direction = spline.getPoint(0).position - spline.getPoint(1).position;
	else
		direction = spline.getPoint(spline.getPointCount() - 1).position - spline.getPoint(spline.getPointCount() - 2).position;

	if (direction.LengthSquared() < 0.0001f)
		return Vector3::UnitZ;

	direction.Normalize();
	return direction;
}
