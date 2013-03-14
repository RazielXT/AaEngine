#include "AaEntity.h"
#include "AaSceneManager.h"
#include <boost/lexical_cast.hpp>
#include "PxRigidDynamic.h"

static int entityNameHelp=0;

AaEntity::AaEntity(AaSceneManager* sceneMgr, AaCamera** cameraPtr)
{
	AaEntity("Entity_name_"+boost::lexical_cast<std::string>(entityNameHelp++),sceneMgr, cameraPtr);
}

AaEntity::AaEntity(std::string name, AaSceneManager* sceneMgr, AaCamera** cameraPtr)
{
	hasPhysics=false;
	hasDynamicPhysics=false;
	dirtyWM=true;
	visible=true;
	mSceneMgr=sceneMgr;
	wMatrix = ( XMMATRIX* )_aligned_malloc( sizeof( XMMATRIX ), 64 ); 
	quaternion = ( XMVECTOR* )_aligned_malloc( sizeof( XMVECTOR ), 16 );
	*quaternion = XMLoadFloat4(&XMFLOAT4(0,0,0,1));
	this->name=name;
	scale=XMFLOAT3(1,1,1);
	position=XMFLOAT3(0.0f, 0.0f, 0.0f );
	mYaw=mPitch=mRoll=0;
	renderQueueOrder=0;
	model=NULL;
	this->cameraPtr=cameraPtr;
	this->material=NULL;
}

AaEntity::~AaEntity()
{
	_aligned_free( ( void* )wMatrix ); 
}

void AaEntity::setMaterial(AaMaterial* material)
{
	this->material=material;

	if(model)
		createInputLayout();
}

XMMATRIX AaEntity::getWorldMatrix()
{
	if(dirtyWM)
	{
		XMMATRIX translationM = XMMatrixTranslation( position.x,position.y,position.z );
		XMMATRIX rotationM;
		if(hasPhysics && hasDynamicPhysics)
			rotationM = XMMatrixRotationQuaternion( *quaternion );
		else
			rotationM = XMMatrixRotationRollPitchYaw( mRoll,mPitch,mYaw );
		XMMATRIX scaleM = XMMatrixScaling(scale.x,scale.y,scale.z);
		*wMatrix=XMMatrixMultiply(XMMatrixMultiply(scaleM,rotationM),translationM);
		dirtyWM=false;
	}

	return *wMatrix;
}

XMMATRIX AaEntity::getWorldViewProjectionMatrix()
{
	return XMMatrixTranspose(XMMatrixMultiply( getWorldMatrix(), *(*cameraPtr)->getViewProjectionMatrix() ));
}

void AaEntity::setPosition(XMFLOAT3 position)
{
	dirtyWM=true;
	this->position=position;

	if(hasPhysics)
	body->setGlobalPose(physx::PxTransform(physx::PxVec3(position.x,position.y,position.z),body->getGlobalPose().q));
}

void AaEntity::setPosition(float x,float y,float z)
{
	dirtyWM=true;
	this->position=XMFLOAT3(x,y,z);

	if(hasPhysics)
	body->setGlobalPose(physx::PxTransform(physx::PxVec3(position.x,position.y,position.z),body->getGlobalPose().q));
}

void AaEntity::setScale(XMFLOAT3 scale)
{
	if(!hasPhysics)
	{
	dirtyWM=true;
	this->scale=scale;
	}
}

XMFLOAT3 AaEntity::getPosition()
{
	return position;
}

XMFLOAT3 AaEntity::getScale()
{
	return scale;
}

XMFLOAT3 AaEntity::getYawPitchRoll()
{
	return XMFLOAT3(mYaw,mPitch,mRoll);
}

void AaEntity::setModel(std::string filename)
{
	model=mSceneMgr->getModel(filename);

	if(model && this->material)
		createInputLayout();

}

void AaEntity::createInputLayout()
{

	HRESULT result = mSceneMgr->getRenderSystem()->getDevice()->CreateInputLayout( model->vertexLayout,
		model->totalLayoutElements, material->shaders[0]->vsBuffer->GetBufferPointer( ), material->shaders[0]->vsBuffer->GetBufferSize( ), &model->inputLayout );

	if( FAILED( result ) )
	{
		AaLogger::getLogger()->writeMessage("ERROR Error creating input layout for entity "+name);
	}
	
}

void AaEntity::setPhysicsTranform(XMFLOAT3 position, XMFLOAT4 orientation)
{
	if(hasPhysics)
	{
		dirtyWM = true;
		*quaternion = XMLoadFloat4(&orientation);
		this->position = position;
	}
}

void AaEntity::removePhysicsBody()
{
	if(hasPhysics)
	{
	physx::PxTransform trans = body->getGlobalPose();
	trans.rotate(physx::PxVec3(0,180,0));
	physx::PxQuat quat = trans.q;

	float x = quat.x;
	float y = quat.y;
	float z = quat.z;
	float w = quat.w;

	mYaw = atan2(2*(y*w - z*x), 1 - 2*y*y - 2*z*z);
	mPitch = asin(2*(x*y + w*z));
	mRoll = atan2(2*(x*w - y*z), 1 - 2*x*x - 2*z*z);

	body->release();
	body = NULL;
	hasPhysics = false;
	dirtyWM = true;
	}
}

void AaEntity::yaw(float yaw) 
{ 
	dirtyWM=true; 
	mYaw+=yaw; 
}

void AaEntity::pitch(float pitch) 
{ 
	dirtyWM=true; 
	mPitch+=pitch; 
}

void AaEntity::roll(float roll) 
{ 
	dirtyWM=true; 
	mRoll+=roll; 
}

void AaEntity::resetRotation() 
{ 
	dirtyWM=true; 
	mYaw=mPitch=mRoll=0; 

	if(hasPhysics)
		body->setGlobalPose(physx::PxTransform(body->getGlobalPose().p,physx::PxQuat::createIdentity()));
}

void AaEntity::setPhysicsBody(physx::PxRigidActor* actor, bool dynamic)
{
	body = actor;
	body->userData = this;
	hasPhysics = true;
	hasDynamicPhysics = dynamic;
}

physx::PxRigidActor* AaEntity::getPhysicsBody()
{
	return body;
}