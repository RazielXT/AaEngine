#include "MyListener.h"
#include "AaLogger.h"
#include "AaModelSerialized.h"
#include "GlobalDefinitions.h"
#include "AaSceneNode.h"
#include "TestGuiWindow.h"
#include "AaSceneParser.h"

AaEntity* ent;
TestGuiWindow* debugWindow;

MyListener::MyListener(AaSceneManager* mSceneMgr, AaPhysicsManager* mPhysicsMgr)
{
	cameraMan = new FreeCamera(mSceneMgr->getCamera());
	mSceneMgr->getCamera()->setPosition(XMFLOAT3(-1.8f,10,6));

	this->mPhysicsMgr = mPhysicsMgr;
	this->mSceneMgr = mSceneMgr;
	continue_rendering = true;

	AaRenderSystem* rs=mSceneMgr->getRenderSystem();
	mSceneMgr->loadMaterialFiles(MATERIAL_DIRECTORY);

	AaMaterial* mat=mSceneMgr->getMaterial("Test2");
	ent= mSceneMgr->createEntity("testEnt",mat);
	ent->setModel("angel");
	ent->setScale(XMFLOAT3(10,10,10));
	ent->setPosition(0,-15,30);


	ent= mSceneMgr->createEntity("testEnt22",mat);
	ent->setModel("angel");
	ent->setScale(XMFLOAT3(10,10,10));
	ent->setPosition(-15,-15,30);
	//ent->yaw(-2.0f);
	AaSceneNode* node= new AaSceneNode(XMFLOAT3(-15,-15,30));
	node->attachEntity(ent);


	ent= mSceneMgr->createEntity("testEnt33",mat);
	ent->setModel("angel");
	ent->setScale(XMFLOAT3(10,10,10));
	ent->setPosition(15,-15,30);
	//ent->yaw(0.7f);
	ent->pitch(-1.0f);	
	AaSceneNode* node2 = node->createChild();
	node2->setPosition(XMFLOAT3(15,-15,30));
	node2->attachEntity(ent);

	node->move(XMFLOAT3(0,10,0));
	node->yawPitchRoll(0.4,0.2,0);
	node->localPitch(1);

	/*ent= mSceneMgr->createEntity("testEnt2",mat);
	ent->setModel("teapot.obj");
	ent->yaw(0.7f);
	ent->pitch(-1.7f);
	ent->yaw(-2.7f);
	ent->setPosition(3,-1,10);*/
	
	PxMaterial* mMaterial;
	mMaterial = mPhysicsMgr->getPhysics()->createMaterial(0.7f, 1.7f, 0.2f);    //static friction, dynamic friction, restitution
	PxShape* aSphereShape;

	ent= mSceneMgr->createEntity("testEnt3",mat);
	ent->setModel("ball32.mesh");
	ent->setPosition(0,10,20);
	ent->setScale(XMFLOAT3(0.4,0.4,0.4));
	mPhysicsMgr->createSphereBodyDynamic(ent,0.4)->setLinearVelocity(physx::PxVec3(1,15,0));

	ent= mSceneMgr->createEntity("testEnt4",mat);
	ent->setModel("ball32.mesh");
	ent->setPosition(0,11,20);
	ent->setScale(XMFLOAT3(0.4,0.4,0.4));
	mPhysicsMgr->createSphereBodyDynamic(ent,0.4)->setLinearVelocity(physx::PxVec3(1,15,0));


	ent= mSceneMgr->createEntity("testEnt6",mat);
	ent->setModel("ball32.mesh");
	ent->setPosition(5,20,20);
	mPhysicsMgr->createConvexBodyDynamic(ent)->setLinearVelocity(physx::PxVec3(1,15,0));


	ent= mSceneMgr->createEntity("testEnt62",mat);
	ent->setModel("ball16.mesh");
	ent->setPosition(5,10,20);
	mPhysicsMgr->createConvexBodyDynamic(ent)->setLinearVelocity(physx::PxVec3(1,15,0));

	ent= mSceneMgr->createEntity("testEnt63",mat);
	ent->setModel("ball32.mesh");
	ent->setPosition(5,5,20);
	ent->setScale(XMFLOAT3(4,1,4));
	ent->yaw(0.5);
	ent->roll(0.5);
	mPhysicsMgr->createTreeBodyStatic(ent);

	ent= mSceneMgr->createEntity("testEnt61",mat);
	ent->setModel("ball24.mesh");
	ent->setPosition(5,15,20);
	ent->setScale(XMFLOAT3(2,1,2));
	ent->roll(0.5);
	mPhysicsMgr->createConvexBodyDynamic(ent)->setLinearVelocity(physx::PxVec3(1,15,0));
	
	Light* l = new Light();
	l->color = XMFLOAT3(1,1,1);
	l->direction = XMFLOAT3(-0.5f,-10,-0.5f);
	XMStoreFloat3(&l->direction,XMVector2Normalize(XMLoadFloat3(&l->direction)));

	mSceneMgr->mShadingMgr->directionalLight = l;

	mPhysicsMgr->createPlane(0,0,0,0);

	loadScene("test.scene",mSceneMgr,mPhysicsMgr);

	mRS=rs;

	setupInputSystem();

	debugWindow = new TestGuiWindow(mSceneMgr->getGuiManager()->getContext());
}

MyListener::~MyListener()
{
}


bool MyListener::frameStarted(float timeSinceLastFrame)
{
	mKeyboard->capture();
	mMouse->capture();
	cameraMan->update(timeSinceLastFrame);

	mSceneMgr->getGuiManager()->update(timeSinceLastFrame/1000.0f);

	mPhysicsMgr->fetchResults(true);
	mPhysicsMgr->synchronizeEntities();

	mPhysicsMgr->startSimulating(timeSinceLastFrame);

	mRS->clearViews();

	mSceneMgr->mShadingMgr->updatePerFrameConstants(timeSinceLastFrame,mSceneMgr->getCamera());
	mSceneMgr->renderScene();

	mSceneMgr->getGuiManager()->render();

	mRS->swapChain_->Present(0,0);

	return continue_rendering;
}


bool MyListener::keyPressed( const OIS::KeyEvent &arg ) 
{
	cameraMan->keyPressed(arg);
	mSceneMgr->getGuiManager()->keyPressed(arg);

	switch (arg.key)
		{

		case OIS::KC_ESCAPE:
			continue_rendering=false;
			break;

         default:
            break;
		}

	return true;
}

bool MyListener::keyReleased( const OIS::KeyEvent &arg ) 
{
	cameraMan->keyReleased(arg);
	mSceneMgr->getGuiManager()->keyReleased(arg);

	std::cout << "KeyReleased {" << ((OIS::Keyboard*)(arg.device))->getAsString(arg.key) << "}\n";
	return true;
}

bool MyListener::mouseMoved( const OIS::MouseEvent &arg ) 
{ 
	cameraMan->mouseMoved(arg);
	mSceneMgr->getGuiManager()->mouseMoved(arg);

	return true;
}

bool MyListener::mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id ) 
{ 
	ent->removePhysicsBody();
	mSceneMgr->getGuiManager()->mousePressed(arg,id);

	if(id == OIS::MB_Right)
		debugWindow->toogleVisibility();

	return true;	
}

bool MyListener::mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id ) 
{ 
	mSceneMgr->getGuiManager()->mouseReleased(arg,id);
	return true;	
}

void MyListener::setupInputSystem()
{

    OIS::ParamList pl;
	std::ostringstream wnd;
	wnd << (size_t)mRS->getWindow()->getHwnd();
    pl.insert(std::make_pair(std::string("WINDOW"), wnd.str()));
    mInputManager = OIS::InputManager::createInputSystem(pl);

	try
    {
        mKeyboard = static_cast<OIS::Keyboard*>(mInputManager->createInputObject(OIS::OISKeyboard, true));
        mMouse = static_cast<OIS::Mouse*>(mInputManager->createInputObject(OIS::OISMouse, true));
		mMouse->getMouseState().width = 1200;
		mMouse->getMouseState().height = 800;

        mMouse->setEventCallback(this);
        mKeyboard->setEventCallback(this);
        //mJoy = static_cast<OIS::JoyStick*>(mInputManager->createInputObject(OIS::OISJoyStick, false));
    }
    catch (const OIS::Exception &e)
    {
        //throw Exception(42, e.eText, "Application::setupInputSystem");
    }
}