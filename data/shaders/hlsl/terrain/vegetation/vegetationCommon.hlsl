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
	float worldMinX;
	float worldMinZ;
	float _pad1;
	float _pad2;
};

static const uint SubgroupsPerDim = 8;
static const uint SubgroupCount = SubgroupsPerDim * SubgroupsPerDim;
static const uint MaxItemsPerSubgroup = 1024;
