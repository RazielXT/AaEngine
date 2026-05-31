#pragma once

#include "ViewportTool.h"
#include "Objects/SplineConstruction.h"
#include "RenderObject/SplineSweep/SplineDebugVisualization.h"

#include <memory>
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

	bool hasRoad() const { return construction != nullptr; }
	bool hasPreview() const { return previewBuilt; }
	bool isPreviewDirty() const { return previewDirty; }
	size_t getSelectedPointIndex() const { return selectedPointIndex; }
	SplineConstruction* getRoad() { return construction.get(); }
	const SplineConstruction* getRoad() const { return construction.get(); }

	float addPointDistance = 12.0f;
	AddPointMode addPointMode = AddPointMode::Auto;
	PointGizmoMode pointGizmoMode = PointGizmoMode::Move;
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
	bool trySelectKnownSplinePart(ObjectId id, Vector3 pickedPosition);
	void markConstructionChanged();
	void markPreviewSettingsChanged();
	void rebuildDebug();
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
	void insertPointBetween(size_t previousPointIndex, size_t nextPointIndex);
	bool shouldExtendStart(size_t pointCount) const;
	Vector3 getForwardAddDirection(bool extendStart) const;

	ApplicationCore& app;
	ViewportPanel& viewport;

	std::unique_ptr<SplineConstruction> construction;
	std::vector<std::unique_ptr<SplineConstruction>> bakedConstructions;
	SplineDebugVisualization debugVisualization;
	SplineDebugVisualizationSettings debugSettings;
	size_t selectedPointIndex = Spline::InvalidPointIndex;
	bool previewBuilt = false;
	bool previewDirty = false;
	bool editModeActive = false;
	bool overlayActive = false;
	bool gizmoActive = false;
};
