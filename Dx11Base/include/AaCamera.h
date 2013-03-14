#ifndef _AA_CAMERA_
#define _AA_CAMERA_

#include<xnamath.h>

class AaCamera
{

public:

	AaCamera()
	{
		//heap allocated XMMATRIX aligned to 16bytes 
		view_m = ( XMMATRIX* )_aligned_malloc( sizeof( XMMATRIX ), 64 ); 
		projection_m = ( XMMATRIX* )_aligned_malloc( sizeof( XMMATRIX ), 64 ); 
		view_projection_m = ( XMMATRIX* )_aligned_malloc( sizeof( XMMATRIX ), 64 ); 

		//initial values
		*view_m = XMMatrixIdentity( );
		*projection_m = XMMatrixPerspectiveFovLH( XM_PIDIV4, 800.0f / 600.0f, 0.01f, 100.0f );
		*view_projection_m = XMMatrixMultiply( *view_m, *projection_m );

		position = XMFLOAT3(0,0,0);
		direction = XMFLOAT3(0,0,1); 
		upVector = XMFLOAT3(0,1,0); 
		yaw_=pitch_=roll_=0;
		dirty=false;
	}

	~AaCamera()
	{	 
		// aligned free 
		_aligned_free( ( void* )view_m ); 
		_aligned_free( ( void* )projection_m ); 
		_aligned_free( ( void* )view_projection_m ); 
	}

	XMMATRIX* getViewProjectionMatrix() 
	{ 
		//if meanwhile moved
		if(dirty)
		{
			*view_m = XMMatrixMultiply(XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&direction), XMLoadFloat3(&upVector) ),XMMatrixRotationY(yaw_));
			*view_m = XMMatrixMultiply(*view_m,XMMatrixRotationX(pitch_));
			*view_m = XMMatrixMultiply(*view_m,XMMatrixRotationZ(roll_));
			*view_projection_m = XMMatrixMultiply( *view_m, *projection_m );
			dirty=false;
		}

		return view_projection_m; 
	}

	void setPosition(XMFLOAT3 position) 
	{ 
		this->position=position; 
		dirty=true; 
	}

	void setInCameraRotation(XMFLOAT3* direction)
	{	
		direction->x*=-1;

		XMVECTOR dir = XMLoadFloat3(direction);
		dir = XMVector3Transform(dir,XMMatrixRotationX(pitch_));
		dir = XMVector3Transform(dir,XMMatrixRotationY(yaw_));	
		XMStoreFloat3(direction,dir);

		direction->x*=-1;
		direction->y*=-1;
	}

	void yaw(float Yaw) 
	{ 
		yaw_ += Yaw;
		dirty = true; 
	}

	void pitch(float Pitch) 
	{ 
		pitch_ += Pitch;
		dirty = true; 
	}

	void roll(float Roll) 
	{ 
		roll_ += Roll;
		dirty = true; 
	}

	void resetRotation() 
	{ 
		yaw_=pitch_=roll_=0;
		dirty = true; 
	}


	void lookTo(XMFLOAT3 Direction) 
	{ 
		direction= Direction;
		yaw_=pitch_=0;
		dirty = true; 
	}

	void lookAt(XMFLOAT3 target) 
	{ 
		direction.x = target.x - position.x;
		direction.y = target.y - position.y;
		direction.z = target.z - position.z;
		yaw_=pitch_=0;
		dirty = true; 
	}

	XMFLOAT3 getPosition() { return position; }
	
	
private:


	bool dirty;
	float yaw_,pitch_,roll_;
	XMMATRIX* view_m;
	XMMATRIX* projection_m;
	XMMATRIX* view_projection_m;
	XMFLOAT3 position;
	XMFLOAT3 direction;
	XMFLOAT3 upVector;

};


#endif