#pragma once

#include <string>

class SplineRoadTool;

class SplineRoadPersistence
{
public:
	void saveSpline(const SplineRoadTool& tool) const;
	void loadSpline(SplineRoadTool& tool) const;
	void bakeAndSaveModel(SplineRoadTool& tool) const;

private:
	bool saveSplineToFile(const SplineRoadTool& tool, const std::string& path) const;
	bool loadSplineFromFile(SplineRoadTool& tool, const std::string& path) const;
	bool saveCurrentMeshModel(const SplineRoadTool& tool, const std::string& path) const;
};