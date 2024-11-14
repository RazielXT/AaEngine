#include "AaEntity.h"
#include "AaModelResources.h"
#include "AaMaterialResources.h"

AaEntity::AaEntity(Renderables& r, std::string_view n) : RenderObject(r), name(n.data())
{
}

AaEntity::AaEntity(Renderables& r, AaEntity& source) : RenderObject(r)
{
	name = source.name;
	material = source.material;
	geometry = source.geometry;

	setTransformation(source.getTransformation(), true);
}

AaEntity::~AaEntity()
{
}
