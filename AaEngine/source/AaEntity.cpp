#include "AaEntity.h"
#include "AaModelResources.h"
#include "AaMaterialResources.h"

static int entityCounter = 0;

AaEntity::AaEntity() : AaEntity("entity" + std::to_string(entityCounter++))
{
}

AaEntity::AaEntity(std::string name)
{
	this->name = name;
}

AaEntity::~AaEntity()
{
}

void AaEntity::setModel(AaModel* m)
{
	model = m;

	setBoundingBox(model->bbox);
}
