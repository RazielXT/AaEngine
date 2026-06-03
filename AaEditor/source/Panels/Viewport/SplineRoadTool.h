#pragma once

#include "ViewportTool.h"
#include "Objects/SplineConstruction.h"
#include "RenderObject/SplineSweep/SplineDebugVisualization.h"

#include <memory>
#include <string>
#include <vector>

class ApplicationCore;
class ViewportPanel;

class SplineRoadTool : public ViewportTool
{
public:
	enum class PreviewProfile
	{
		Road,
		Rectangle,
		Circle,
		Tube,
		Arc,
		FilledArc,
	};

	enum class AddPointMode
	{
		Auto,
		InsertAfter,
		InsertBefore,
		ExtendStart,
		ExtendEnd,
	};

	enum class PointGizmoMode
	{
		Move,
		Roll,
		SplineTransform,
	};

	enum class ReplicateCurveMode
	{
		Drift,
		Loop,
	};

	SplineRoadTool(ApplicationCore& app, ViewportPanel& viewport);
	~SplineRoadTool() override;

	void onPick(const EntityPicker::PickInfo& pickInfo, bool ctrlActive, Camera& camera) override;
	void draw(const ViewportToolContext& ctx) override;
	void onActivated() override;
	void onDeactivated() override;

	bool isInteracting() const override { return gizmoActive; }
	bool isOverlayActive() const override { return overlayActive; }
	void reset() override;

	void createRoad();
	void clearRoad();
	void addPointForward();
	void removeSelectedPoint();
	void buildPreview();
	void bakePreview();
	void saveSpline();
	void loadSpline();
	void bakeAndSaveModel();
	void copyActiveSpline();
	bool selectPathForMovement(ObjectId id, Vector3 pickedPosition);
	void enterPointEditMode();
	void enterSplineTransformMode();
	bool isPointEditMode() const { return pointGizmoMode != PointGizmoMode::SplineTransform; }
	bool isSplineTransformMode() const { return pointGizmoMode == PointGizmoMode::SplineTransform; }
	void selectPreviousPoint();
	void selectNextPoint();

	bool hasRoad() const { return getRoad() != nullptr; }
	bool hasPreview() const;
	bool isPreviewDirty() const;
	size_t getSelectedPointIndex() const { return selectedPointIndex; }
	SplineConstruction* getRoad();
	const SplineConstruction* getRoad() const;
	size_t getSplineCount() const { return splines.size(); }
	size_t getActiveSplineListIndex() const { return activeSplineIndex; }
	uint32_t getActiveSplineId() const;
	const char* getActiveSplineName() const;
	const char* getSplineName(size_t index) const;
	void selectSplineByIndex(size_t index, bool movementMode = false);

	float addPointDistance = 12.0f;
	AddPointMode addPointMode = AddPointMode::Auto;
	PointGizmoMode pointGizmoMode = PointGizmoMode::Move;
	bool replicateCurveOnAdd = false;
	ReplicateCurveMode replicateCurveMode = ReplicateCurveMode::Drift;
	bool autoBuildPreview = false;
	bool previewPhysics = false;
	PreviewProfile previewProfile = PreviewProfile::Road;
	float profileWidth = 8.0f;
	int roadWidthSegments = 1;
	float profileHeight = 1.0f;
	float profileRadius = 4.0f;
	float profileInnerRadius = 3.2f;
	float profileArcAngleDegrees = 180.0f;
	int profileSegments = 16;
	bool preventProfileFoldover = true;
	float minProfileForwardScale = 0.08f;
	float curvatureTessellationScale = 2.0f;
	float rollTessellationScale = 6.0f;
	int maxTessellationSegments = 24;
	bool showDebugPath = true;
	bool showDebugPoints = true;
	bool showActiveFrame = true;

	void setSelectedPointPosition(Vector3 position);
	void setSelectedPointRoll(float roll);
	void setSelectedPointUpStabilization(float upStabilization);
	void onAutoBuildPreviewChanged();
	void onProfileSettingsChanged();
	void onSplineSamplingSettingsChanged();
	void refreshDebugVisualization();

private:
	struct SplineRoadInstance
	{
		uint32_t id = 0;
		std::string name;
		std::unique_ptr<SplineConstruction> construction;
		SplineDebugVisualization debugVisualization;
		bool previewBuilt = false;
		bool previewDirty = false;
	};

	SplineRoadInstance* activeSpline();
	const SplineRoadInstance* activeSpline() const;
	SplineRoadInstance* findSplineByPathEntity(RenderEntity* entity);
	SplineRoadInstance& createSplineInstance();
	void removeSplineFromWorld(SplineRoadInstance& instance);
	bool trySelectKnownSplinePart(ObjectId id, Vector3 pickedPosition);
	void markConstructionChanged();
	void markPreviewSettingsChanged();
	void rebuildDebug();
	void rebuildDebug(SplineRoadInstance& instance);
	void rebuildAllDebug();
	void removePreview();
	bool saveSplineToFile(const std::string& path) const;
	bool loadSplineFromFile(const std::string& path);
	bool saveCurrentMeshModel(const std::string& path) const;
	void updateDebugSettings();
	void applySplineSamplingSettings();
	void ensureRoad();
	ShapeProfile2D createPreviewProfile() const;
	void drawSelectedPointGizmo(const ViewportToolContext& ctx);
	void drawSelectedPointMoveGizmo(const ViewportToolContext& ctx);
	void drawSelectedPointRollGizmo(const ViewportToolContext& ctx);
	void drawSplineTransformGizmo(const ViewportToolContext& ctx);
	void insertPointBetween(size_t previousPointIndex, size_t nextPointIndex);
	void selectPointByOffset(int offset);
	bool addReplicatedCurvePoint(bool extendStart);
	bool shouldExtendStart(size_t pointCount) const;
	Vector3 getForwardAddDirection(bool extendStart) const;

	ApplicationCore& app;
	ViewportPanel& viewport;

	std::vector<std::unique_ptr<SplineRoadInstance>> splines;
	std::vector<std::unique_ptr<SplineConstruction>> bakedConstructions;
	SplineDebugVisualizationSettings debugSettings;
	size_t activeSplineIndex = Spline::InvalidPointIndex;
	uint32_t nextSplineId = 1;
	size_t selectedPointIndex = Spline::InvalidPointIndex;
	bool editModeActive = false;
	bool overlayActive = false;
	bool gizmoActive = false;
};
