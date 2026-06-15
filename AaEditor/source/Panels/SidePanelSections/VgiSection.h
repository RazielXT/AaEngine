#pragma once

class ApplicationCore;
struct DebugState;

class VgiSection
{
public:
	void draw(ApplicationCore& app, DebugState& state);

private:
	bool showVoxels = false;
	int showVoxelsIndex = 0;
	int showVoxelsMip = 0;
};
