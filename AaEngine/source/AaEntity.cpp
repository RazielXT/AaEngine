#include "AaEntity.h"
#include "AaModelResources.h"
#include "AaMaterialResources.h"

AaEntity::AaEntity(Renderables& r, std::string name) : RenderObject(r)
{
	this->name = name;
}

AaEntity::~AaEntity()
{
}
