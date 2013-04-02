#ifndef _PREVIEW_CAM_
#define _PREVIEW_CAM_

#include <windows.h>
#include <vector>

#include "OIS.h"
#include "AaCamera.h"


class FreeCamera
{

public:
	
	FreeCamera(AaCamera* cam)
	{
		camera = cam;
		w=s=a=d=turbo=false;
		mouseX=mouseY=0;
	}

	~FreeCamera(){}

	void update(float time)
	{
		XMFLOAT3 dir(0,0,0);
		float speed=4*time;
		if(turbo) speed*=6;

		if (w || s || a || d)
		{
			if (!a && !d) 
			{
				if (w){dir.z=speed;}
				if (s) {dir.z=-speed;}
			} 
			else  
			{
				if (d)
				{
					if (!w && !s) {dir.x=speed;} else
						if (w) {dir.x=0.7*speed; dir.z=0.7*speed;} else
							if (s) {dir.x=0.7*speed; dir.z=-0.7*speed;} 
				}

				if (a)
				{
					if (!w && !s) {dir.x=-speed;} else
						if (w) {dir.x=-0.7*speed; dir.z=0.7*speed;} else
							if (s) {dir.x=-0.7*speed; dir.z=-0.7*speed;} 
				}
			}  

			camera->setInCameraRotation(&dir);
			
			dir.x+=camera->getPosition().x;
			dir.y+=camera->getPosition().y;
			dir.z+=camera->getPosition().z;

			camera->setPosition(dir);
		}
	}

	void keyPressed( const OIS::KeyEvent &arg )
	{
		switch (arg.key)
		{
		case OIS::KC_W: 
			w=true;
			break;
		case OIS::KC_S: 
			s=true;
			break;
		case OIS::KC_A: 
			a=true;
			break;
		case OIS::KC_D: 
			d=true;
			break;
		case OIS::KC_LSHIFT: 
			turbo=true;
			break;
		}
	}

	void keyReleased( const OIS::KeyEvent &arg )
	{
		switch (arg.key)
		{
		case OIS::KC_W: 
			w=false;
			break;
		case OIS::KC_S: 
			s=false;
			break;
		case OIS::KC_A: 
			a=false;
			break;
		case OIS::KC_D: 
			d=false;
			break;
		case OIS::KC_LSHIFT: 
			turbo=false;
			break;
		}
	}

	void mouseMoved( const OIS::MouseEvent &arg ) 
	{ 	
		camera->yaw(-0.001*arg.state.X.rel);
		camera->pitch(-0.001*arg.state.Y.rel);
	}

private:

	AaCamera* camera;
	float mouseX,mouseY;
	bool w,s,a,d,turbo;
};


#endif