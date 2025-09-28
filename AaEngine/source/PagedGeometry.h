#pragma once

#include "GraphicsResources.h"
#include "PagedGeometryLoader.h"

class PagedGeometry
{
public:

	PagedGeometry(GraphicsResources&);

	void addLoader(std::shared_ptr<PagedGeometryLoader>);

	void update(const Camera&);

	void reset();

private:

	std::vector<std::shared_ptr<PagedGeometryLoader>> loaders;
};