struct VegetationInfo
{
	float3 position;
	float rotation;
	float scale;
	float random;
};

struct SubgroupMeta
{
	int counter;
	int minY;
	int maxY;
	int _pad;
};

static const uint SubgroupsPerDim = 8;
static const uint SubgroupCount = SubgroupsPerDim * SubgroupsPerDim;
static const uint MaxItemsPerSubgroup = 1024;
