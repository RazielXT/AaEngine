#ifndef TESTGUI_H
#define TESTGUI_H

#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Core/EventListener.h>
#include "GlobalDefinitions.h"

class TestGuiWindow : public Rocket::Core::EventListener
{
public:
	TestGuiWindow(Rocket::Core::Context* context);
	virtual ~TestGuiWindow();

	// Process the incoming event.
	virtual void ProcessEvent(Rocket::Core::Event& event);
	void toogleVisibility(); 

private:

	bool visible;
	Rocket::Core::Context* context;
	Rocket::Core::ElementDocument* document;
	Rocket::Core::Element* button;
	Rocket::Core::Element* text;
	Rocket::Core::Element* checkB;
	Rocket::Controls::ElementFormControlInput* inputText;

	Rocket::Core::String innRml;
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

TestGuiWindow::TestGuiWindow(Rocket::Core::Context* context)
{
	visible=true;
	this->context = context;

	 document = context->LoadDocument( Rocket::Core::String(GUI_DIRECTORY) + "demo.rml");
	
	if (document != NULL)
	{
		document->Show();
		document->RemoveReference();
	}

	document->GetElementById("title")->SetInnerRML("Debug Output");

	button = document->GetElementById("window")->GetElementById("button_input");
	text = document->GetElementById("window")->GetElementById("text_output");
	text->SetInnerRML("Hello");

	button->AddEventListener("click",this);
	text->AddEventListener("dblclick",this);

	checkB = document->GetElementById("window")->GetElementById("reverb");

	checkB->AddEventListener("click",this);

	inputText = dynamic_cast< Rocket::Controls::ElementFormControlInput* >(document->GetElementById("window")->GetElementById("player_input"));
}

TestGuiWindow::~TestGuiWindow()
{
	context->UnloadDocument(document);
}

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
}

#endif