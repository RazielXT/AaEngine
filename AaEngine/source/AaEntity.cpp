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

void AaEntity::setModel(std::string filename)
{
	model = AaModelResources::get().getModel(filename);

	setBoundingBox(model->bbox);
}

void AaEntity::setMaterial(std::string name)
{
	material = AaMaterialResources::get().getMaterial(name)->Assign(model->vertexLayout);
	depthMaterial = AaMaterialResources::get().getMaterial("earlyZ")->Assign(model->vertexLayout);
}
