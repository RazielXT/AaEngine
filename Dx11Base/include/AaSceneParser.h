#ifndef _SCENEPARSER_
#define _SCENEPARSER_

#include <algorithm>
#include <string> 
//#include <boost/algorithm/string/>
#include <boost/lexical_cast.hpp>


struct SceneNode
{
	XMFLOAT3 position;
	XMFLOAT4 orientation;
	XMFLOAT3 scale;
};

const TiXmlElement* IterateChildElements(const TiXmlElement* xmlElement, const TiXmlElement* childElement)
{
    if (xmlElement != 0)
    {
        if (childElement == 0)
            childElement = xmlElement->FirstChildElement();
        else
            childElement = childElement->NextSiblingElement();
        
        return childElement;
    }

    return 0;
}

bool getBool(std::string value)
{
    std::string valueLower = value;
	std::transform(valueLower.begin(), valueLower.end(), valueLower.begin(), ::tolower);

    if (valueLower == "false" || value == "no" || value == "0")
        return false;
    else
        return true;
}   

float GetRealAttribute(const TiXmlElement* xmlElement, const char* name, float defaultValue = 0)
{
    std::string value = xmlElement->Attribute(name);
    return value.empty() ? defaultValue : boost::lexical_cast<float>(value);    
}

int GetIntAttribute(const TiXmlElement* xmlElement, const char* name, int defaultValue = 0)
{
	std::string value = xmlElement->Attribute(name);
	return value.empty() ? defaultValue : boost::lexical_cast<int>(value);    
}

XMFLOAT3 LoadXYZ(const TiXmlElement* objectElement)
{
    XMFLOAT3 xyz;
    xyz.x = GetRealAttribute(objectElement, "x");
    xyz.y = GetRealAttribute(objectElement, "y");
    xyz.z = GetRealAttribute(objectElement, "z");
    return xyz;
}

LightType ParseLightType(std::string type)
{
	std::string typeLower = type;
	std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);

    if (typeLower == "point")
        return LightType_Point;
    else if (typeLower == "directional")
        return LightType_Directional;
    else if (typeLower == "spot")
        return LightType_Spotlight;

	return LightType_Point;
}

XMFLOAT4 LoadRotation(const TiXmlElement* objectElement)
{
    XMFLOAT4 rotation(0,0,0,1);

        rotation.x = GetRealAttribute(objectElement, "qx", 1);
        rotation.y = GetRealAttribute(objectElement, "qy", 0);
        rotation.z = GetRealAttribute(objectElement, "qz", 1);
        rotation.w = GetRealAttribute(objectElement, "qw", 0);
   
	return rotation;
}

XMFLOAT3 LoadColor(const TiXmlElement* objectElement)
{   
    XMFLOAT3 color;
    color.x = GetRealAttribute(objectElement, "r", 0);
    color.y = GetRealAttribute(objectElement, "g", 0);
    color.z = GetRealAttribute(objectElement, "b", 0);
    //color.w = 1;
    return color;
}

std::string GetStringAttribute(const TiXmlElement* xmlElement, const char* name)
{
    const char* value = xmlElement->Attribute(name);
    if (value != 0)
        return value;
    else
        return "";    
}

void LoadLightAttenuation(const TiXmlElement* objectElement, Light* light)
{
	
    std::string value;

    value = GetStringAttribute(objectElement, "range");
    float range = value.empty() ? light->attenuation : boost::lexical_cast<float>(value);
	/*
    value = GetStringAttribute(objectElement, "constant");
    Real constant = value.empty() ? light->getAttenuationConstant() : StringConverter::parseReal(value);

    value = GetStringAttribute(objectElement, "linear");
    Real linear = value.empty() ? light->getAttenuationLinear() : StringConverter::parseReal(value);

    value = GetStringAttribute(objectElement, "quadric");
    Real quadric = value.empty() ? light->getAttenuationQuadric() : StringConverter::parseReal(value);
    */

}

void LoadLightRange(const TiXmlElement* objectElement, Light* light)
{

    if (light->type == LightType_Spotlight)
    {
        std::string value;

      /*  value = GetStringAttribute(objectElement, "inner");
        if (!value.empty())
            light->setSpotlightInnerAngle(Radian(StringConverter::parseReal(value)));

        value = GetStringAttribute(objectElement, "outer");
        if (!value.empty())
            light->setSpotlightOuterAngle(Radian(StringConverter::parseReal(value)));

        value = GetStringAttribute(objectElement, "falloff");
        if (!value.empty())
            light->setSpotlightFalloff(StringConverter::parseReal(value));*/
    }
}

bool AllDigits(std::string text)
{
    for (size_t i = 0; i < text.length(); i++)
    {
        if (!isdigit(text[i]))
            return false;
    }

    return true;
}

UCHAR ParseRenderQueue(std::string renderQueue)
{
   /* static std::map<String, uint8> nameToNumber;
    if (nameToNumber.empty())
    {
        nameToNumber["background"] = RENDER_QUEUE_BACKGROUND;
        nameToNumber["skiesearly"] = RENDER_QUEUE_SKIES_EARLY;
        nameToNumber["queue1"] = RENDER_QUEUE_1;
        nameToNumber["queue2"] = RENDER_QUEUE_2;
		nameToNumber["worldgeometry1"] = RENDER_QUEUE_WORLD_GEOMETRY_1;
        nameToNumber["queue3"] = RENDER_QUEUE_3;
        nameToNumber["queue4"] = RENDER_QUEUE_4;
		nameToNumber["main"] = RENDER_QUEUE_MAIN;
        nameToNumber["queue6"] = RENDER_QUEUE_6;
        nameToNumber["queue7"] = RENDER_QUEUE_7;
		nameToNumber["worldgeometry2"] = RENDER_QUEUE_WORLD_GEOMETRY_2;
        nameToNumber["queue8"] = RENDER_QUEUE_8;
        nameToNumber["queue9"] = RENDER_QUEUE_9;
        nameToNumber["skieslate"] = RENDER_QUEUE_SKIES_LATE;
        nameToNumber["overlay"] = RENDER_QUEUE_OVERLAY;
		nameToNumber["max"] = RENDER_QUEUE_MAX;
    }

    if (renderQueue.empty())
        return RENDER_QUEUE_MAIN;
    else if (AllDigits(renderQueue))
        return (uint8)StringConverter::parseUnsignedInt(renderQueue);
    else
    {
        //The render queue name, lowercase
        String renderQueueLower;

        //Get the offset that comes after the +, if any
        uint8 offset = 0;
        size_t plusFoundAt = renderQueue.find('+');
        if (plusFoundAt != String::npos)
        {
            //Parse the number
            String offsetText = renderQueue.substr(plusFoundAt + 1);
            StringUtil::trim(offsetText);

            offset = (uint8)StringConverter::parseUnsignedInt(offsetText);

            //Remove the "+offset" substring from the render queue name
            renderQueueLower = renderQueue.substr(0, plusFoundAt);
            StringUtil::trim(renderQueueLower);
        }
        else
            renderQueueLower = renderQueue;
        StringUtil::toLowerCase(renderQueueLower);

        //Look up the render queue and return it
        std::map<String, uint8>::iterator item = nameToNumber.find(renderQueueLower);
        if (item != nameToNumber.end())
        {
            //Don't let the render queue exceed the maximum
            return std::min((uint8)(item->second + offset), (uint8)RENDER_QUEUE_MAX);
        }
        else
        {
            StringUtil::StrStreamType errorMessage;
            errorMessage << "Invalid render queue specified: " << renderQueue;

            OGRE_EXCEPT
                (
                Exception::ERR_INVALIDPARAMS,
	            errorMessage.str(),
	            "OgreMaxUtilities::ParseRenderQueue"
                );
        }
    }*/

	return 1;
}

void GetChildText(const TiXmlElement* xmlElement, std::string text)
{
    //Get the first element 
    const TiXmlNode* childNode = xmlElement->FirstChild();
    while (childNode != 0)
    {
        if (childNode->Type() == TiXmlNode::TEXT)
        {
            const TiXmlText* textNode = childNode->ToText();
            if (textNode != 0)
            {
                text = textNode->Value();
                break;
            }
        }
        childNode = xmlElement->NextSibling();
    }    
}
/*
void loadPlane(const TiXmlElement* planeElement,SceneNode* node, Ogre::SceneManager *mSceneMgr)
{   


	Ogre::Vector3 normal;
	Ogre::Vector3 upVector;

    const TiXmlElement* childElement = 0;
    while (childElement = IterateChildElements(planeElement, childElement))
    {
	

        String elementName = childElement->Value();
      
		if (elementName == "normal")
			normal=LoadXYZ(childElement);
        else if (elementName == "upVector")
			upVector=LoadXYZ(childElement);
       
    }

	Ogre::Plane plane(normal, GetRealAttribute(planeElement, "distance"));
	MeshManager::getSingleton().createPlane
        (
        planeElement->Attribute("name"), 
		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        plane,
        GetRealAttribute(planeElement, "width"), GetRealAttribute(planeElement, "height"),
        GetIntAttribute(planeElement, "xSegments"), GetIntAttribute(planeElement, "ySegments"),
        getBool(planeElement->Attribute("normals")), GetIntAttribute(planeElement, "numTexCoordSets"),
        GetRealAttribute(planeElement, "uTile"), GetRealAttribute(planeElement, "vTile"),
        upVector
        );

	Ogre::Entity* ent;
	ent= mSceneMgr->createEntity( planeElement->Attribute("name"), planeElement->Attribute("name") );
	ent->getMesh()->buildTangentVectors();
	node->attachObject( ent );
	ent->setCastShadows(getBool(planeElement->Attribute("castShadows")));
	ent->setMaterialName(planeElement->Attribute("material"));
	ent->setVisibilityFlags(1);

	Ogre::uint8 renderQueue=ParseRenderQueue(GetStringAttribute(planeElement,"renderQueue"));
	ent->setRenderQueueGroup(renderQueue);
}*/

void loadLight(const TiXmlElement* lightElement,SceneNode* node, AaSceneManager *mSceneMgr)
{   

    //Create the light
    Light* light = new Light();
    light->type = ParseLightType(GetStringAttribute(lightElement, "type"));
	light->position = node->position;
	XMFLOAT3 unitV(0,0,1);
	XMVECTOR v = XMVector3Rotate(XMLoadFloat3(&unitV),XMLoadFloat4(&node->orientation));
	XMStoreFloat3(&light->direction,v);

    //light->setCastShadows(getBool(lightElement->Attribute("castShadows")));
    //light->setPowerScale(GetRealAttribute(lightElement, "power"));

    const TiXmlElement* childElement = 0;
    while (childElement = IterateChildElements(lightElement, childElement))
    {
        std::string elementName = childElement->Value();
      
		if (elementName == "colourDiffuse")
			light->color = LoadColor(childElement);
       /* else if (elementName == "colourSpecular")
			light->setSpecularColour(LoadColor(childElement));
        else if (elementName == "lightRange")
			LoadLightRange(childElement, light);
        else if (elementName == "lightAttenuation")
            LoadLightAttenuation(childElement, light);
        else if (elementName == "position")
            light->setPosition(LoadXYZ(childElement));
        else if (elementName == "normal")
            light->setDirection(LoadXYZ(childElement));  */  
    }
}


void loadEntity(const TiXmlElement* entityElement,SceneNode* node, bool visible, AaSceneManager *mSceneMgr,AaPhysicsManager* mWorld)
{

	std::string name = entityElement->Attribute("name");
	
	std::string file = entityElement->Attribute("meshFile");

	//Ogre::LogManager::getSingleton().getLog("Loading.log")->logMessage("Entity "+ent->getName(),LML_NORMAL);

	
	std::string str = GetStringAttribute(entityElement,"castShadows");
	bool castShadows = true;
	if( !str.empty() && !getBool(str)) castShadows = false;
	
	/*String str2 = GetStringAttribute(entityElement,"visibilityFlags");
	if(!str2.empty()) 
	{
		ent->setVisibilityFlags(Ogre::StringConverter::parseLong(str2));
	}
	else
	{
		ent->setVisibilityFlags(1);
	}*/

	UCHAR renderQueue=ParseRenderQueue(GetStringAttribute(entityElement,"renderQueue"));
	//ent->setRenderQueueGroup(renderQueue);

	AaEntity* ent = NULL;
	const TiXmlElement* subentityElement=entityElement->FirstChildElement("subentities");
    const TiXmlElement* childElement = 0;
    while (childElement = IterateChildElements(subentityElement, childElement))
    {
		ent = mSceneMgr->createEntity(name , GetStringAttribute(childElement, "materialName") );
		ent->setModel(file);
		ent->setPosition(node->position);
		ent->setScale(node->scale);
		ent->setQuaternion(node->orientation);

		//ent->setVisible(visible);
        int index = GetIntAttribute(childElement, "index", 0);
        ent->castShadows = castShadows;
		break;
    }


	const TiXmlElement* userdataElement=entityElement->FirstChildElement("userData");

	if (userdataElement!=NULL)
	{

	std::string userData;
    GetChildText(userdataElement,userData);

	//Ogre::Log* myLog = Ogre::LogManager::getSingleton().getLog("Loading.log");
	//myLog->logMessage(userData,LML_NORMAL);

	TiXmlDocument document;
	document.Parse(userData.c_str());
	if (!document.Error())
	{
		TiXmlElement *root=document.RootElement();

		if (root->Value()!=NULL)
		{
			std::string rootTag(root->Value());
			TiXmlElement *element=root->FirstChildElement();

			//if (rootTag == "PhysicalBody")
		
		}
	}
}

}

void loadNode(const TiXmlElement* nodeElement, AaSceneManager *mSceneMgr,AaPhysicsManager* mWorld)
{

	SceneNode node;
	std::string name;
	name = nodeElement->Attribute("name");

		
	bool visible=true;
	if(nodeElement->Attribute("visibility")!=NULL)
	{
		std::string v=nodeElement->Attribute("visibility");
	
		if (v=="hidden") visible=false; 
	}
	

	//first properties, then content
	std::string elementName;
	const TiXmlElement* childElement = 0;


	while (childElement = IterateChildElements(nodeElement, childElement))
    {
        elementName = childElement->Value();
        
        if (elementName == "position")
            node.position = (LoadXYZ(childElement));
        else if (elementName == "rotation")
            node.orientation = (LoadRotation(childElement));
        else if (elementName == "scale")
            node.scale = (LoadXYZ(childElement));
		/*else if (elementName == "animations")
            loadAnimations(childElement, node, mSceneMgr);*/
	}


	childElement = 0;

    while (childElement = IterateChildElements(nodeElement, childElement))
    {
		elementName = childElement->Value();

        if (elementName == "entity")
            loadEntity(childElement, &node,visible, mSceneMgr, mWorld);
        else if (elementName == "light")
            loadLight(childElement, &node,mSceneMgr);
		/*else if (elementName == "plane")
            loadPlane(childElement, node,mSceneMgr);*/
    }

}


void loadScene(std::string filename, AaSceneManager *mSceneMgr,AaPhysicsManager* mWorld)
{

	filename.insert(0,MODEL_DIRECTORY);

	//Ogre::LogManager::getSingleton().getLog("Loading.log")->logMessage("LOADING SCENE :: filename \""+filename+"\"",LML_NORMAL);

	TiXmlDocument document;
	document.LoadFile(filename.c_str());
	const TiXmlElement* mainElement = document.RootElement();
	const TiXmlElement* nodesElement = mainElement->FirstChildElement("nodes");

	std::string elementName;
    const TiXmlElement* childElement = 0;
    while (childElement = IterateChildElements(nodesElement, childElement))
    {
        elementName = childElement->Value();
        
        if (elementName == "node")
            loadNode(childElement,mSceneMgr,mWorld);
        /*else if (elementName == "skyBox")
            LoadSkyBox(childElement);*/
    }

	//Ogre::LogManager::getSingleton().getLog("Loading.log")->logMessage("LOADING COMPLETED :: filename \""+filename+"\"",LML_NORMAL);
	//Ogre::LogManager::getSingleton().getLog("Loading.log")->logMessage("-----------------------------------------------------------",LML_NORMAL);
}


#endif