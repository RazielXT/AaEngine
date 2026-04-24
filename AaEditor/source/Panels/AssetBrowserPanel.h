#pragma once

#include <string>
#include <vector>

class AssetBrowserPanel
{
public:

	AssetBrowserPanel();

	void draw();

private:

	enum class AssetType { Model, Scene, Prefab };

	struct AssetItem
	{
		std::string name;
		std::string path;
		AssetType type;
	};

	struct Filter
	{
		char search[128] = "";
		bool showModel = true;
		bool showScene = true;
		bool showPrefab = true;
	};

	std::vector<AssetItem> assets;
	Filter filter;

	void lookupAssets();
	void drawTopBar();
	bool passesFilter(const AssetItem& asset) const;
};
