#pragma once

class ApplicationCore;

class VctSection
{
public:
	void draw(ApplicationCore& app);

private:
	bool showVoxels = false;
	int showVoxelsIndex = 0;
	int showVoxelsMip = 0;
};
