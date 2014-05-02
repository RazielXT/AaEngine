#ifndef TESTGUI_H
#define TESTGUI_H

#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <..\Source\Controls\InputTypeRange.h>
#include <Rocket/Core/EventListener.h>
#include "GlobalDefinitions.h"


class TestGuiWindow : public Rocket::Core::EventListener
{
public:
	TestGuiWindow(Rocket::Core::Context* context, AaSceneManager* mSceneMgr);
	virtual ~TestGuiWindow();

	void updateFPS(float tslf);

	// Process the incoming event.
	virtual void ProcessEvent(Rocket::Core::Event& event);
	void toogleVisibility(); 
	bool visible;
private:

	AaSceneManager* mSceneMgr;
	
	Rocket::Core::Context* context;
	Rocket::Core::ElementDocument* document;
	Rocket::Core::Element* button;
	Rocket::Core::Element* text;
	Rocket::Core::Element* checkB;
	Rocket::Controls::ElementFormControlInput* inputText;

	Rocket::Controls::ElementFormControlInput* inputSC;
	Rocket::Controls::ElementFormControlInput* inputCS;
	Rocket::Controls::ElementFormControlInput* inputCD;
	Rocket::Controls::ElementFormControlInput* inputSS;
	Rocket::Controls::ElementFormControlInput* inputSD;

	Rocket::Core::String innRml;
	Rocket::Core::String innRml2;

	Rocket::Core::String fps;
	float fpsUpdateDelay;
};

void TestGuiWindow::toogleVisibility()
{
	if(visible)
	{
		document->Hide();
		context->ShowMouseCursor(false);
	}
	else
	{
		document->Show();
		context->ShowMouseCursor(true);
	}

	visible=!visible;
}

TestGuiWindow::TestGuiWindow(Rocket::Core::Context* context, AaSceneManager* mSceneMgr)
{
	visible=true;
	this->context = context;
	this->mSceneMgr = mSceneMgr;

	 document = context->LoadDocument( Rocket::Core::String(GUI_DIRECTORY) + "demo.rml");
	
	if (document != NULL)
	{
		document->Show();
		document->RemoveReference();
	}

	document->GetElementById("title")->SetInnerRML("Debug Output");

	//button = document->GetElementById("window")->GetElementById("button_input");
	//text = document->GetElementById("window")->GetElementById("text_output");
	//text->SetInnerRML("Hello");

	//button->AddEventListener("click",this);
	//text->AddEventListener("dblclick",this);
	document->GetElementById("window")->GetElementById("cs")->AddEventListener("change",this);
	document->GetElementById("window")->GetElementById("cd")->AddEventListener("change",this);
	document->GetElementById("window")->GetElementById("ss")->AddEventListener("change",this);
	document->GetElementById("window")->GetElementById("sd")->AddEventListener("change",this);
	document->GetElementById("window")->GetElementById("scale")->AddEventListener("change",this);

	inputCS = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("cs"));
	inputCD = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("cd"));
	inputSS = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("ss"));
	inputSD = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("sd"));
	inputSC = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("scale"));

	inputSC->SetValue("1");
	inputCS->SetValue("0.7");
	inputCD->SetValue("0.7");
	inputSS->SetValue("0.8");
	inputSD->SetValue("0.58");

	//checkB = document->GetElementById("window")->GetElementById("reverb");
	//checkB->AddEventListener("click",this);
	fpsUpdateDelay = 0;

	//inputText = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("player_input"));
}

TestGuiWindow::~TestGuiWindow()
{
	context->UnloadDocument(document);
}

void TestGuiWindow::updateFPS(float tslf)
{
	int fpsInt = 1/tslf;
	fpsUpdateDelay+=tslf;

	if (fpsUpdateDelay>0.25f)
	{
		fps.FormatString(12,"FPS: %d",fpsInt);
		fpsUpdateDelay=0;
	}
	
	document->GetElementById("title")->SetInnerRML(fps);
}

float mc[2] = {0.8f};
float sc[2] = {0.8f};

void TestGuiWindow::ProcessEvent(Rocket::Core::Event& event)
{
	if(!visible)
		return;

	Rocket::Core::String source = event.GetCurrentElement()->GetId();

	if(event=="click")
	{
		if(source=="button_input")
		{
			Rocket::Core::Variant* v = inputText->GetAttribute("value");
			innRml.Append(v->Get<Rocket::Core::String>());
			text->SetInnerRML(innRml);
		}
		else
		if(source=="reverb")
		{
			innRml.Append("clicko,");
			text->SetInnerRML(innRml);
		}
	}

	if(event=="dblclick")
	{
		if(source=="text_output")
		{
			innRml.Clear();
			text->SetInnerRML("cleared");
		}

	}


	if(event=="change")
	{
		if(source=="scale")
		{
			float f = boost::lexical_cast<float>(inputSC->GetValue().CString());

			mSceneMgr->getMaterial("White")->setMaterialConstant("radius",Shader_type_pixel,&f);

			document->GetElementById("window")->GetElementById("sct")->SetInnerRML(inputSC->GetValue());
		}

		if(source=="cs")
		{
			mc[0] = boost::lexical_cast<float>(inputCS->GetValue().CString());
			document->GetElementById("window")->GetElementById("cst")->SetInnerRML(inputCS->GetValue());
		}

		if(source=="cd")
		{
			mc[1] = boost::lexical_cast<float>(inputCD->GetValue().CString());
			document->GetElementById("window")->GetElementById("cdt")->SetInnerRML(inputCD->GetValue());
		}

		if(source=="ss")
		{
			sc[0] = boost::lexical_cast<float>(inputSS->GetValue().CString());
			document->GetElementById("window")->GetElementById("sst")->SetInnerRML(inputSS->GetValue());
		}

		if(source=="sd")
		{
			sc[1] = boost::lexical_cast<float>(inputSD->GetValue().CString());
			document->GetElementById("window")->GetElementById("sdt")->SetInnerRML(inputSD->GetValue());
		}

		mSceneMgr->getMaterial("White")->setMaterialConstant("middleCone",Shader_type_pixel,mc);
		mSceneMgr->getMaterial("White")->setMaterialConstant("sideCone",Shader_type_pixel,sc);

		mSceneMgr->getMaterial("Red")->setMaterialConstant("middleCone",Shader_type_pixel,mc);
		mSceneMgr->getMaterial("Red")->setMaterialConstant("sideCone",Shader_type_pixel,sc);

		mSceneMgr->getMaterial("Blue")->setMaterialConstant("middleCone",Shader_type_pixel,mc);
		mSceneMgr->getMaterial("Blue")->setMaterialConstant("sideCone",Shader_type_pixel,sc);

		mSceneMgr->getMaterial("Green")->setMaterialConstant("middleCone",Shader_type_pixel,mc);
		mSceneMgr->getMaterial("Green")->setMaterialConstant("sideCone",Shader_type_pixel,sc);

	}
}

#endif