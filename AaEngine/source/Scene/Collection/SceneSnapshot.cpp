#include "SceneSnapshot.h"
#include <pugixml.hpp>

namespace
{
	pugi::xml_node writeVec3(pugi::xml_node parent, const char* name, const Vector3& v)
	{
		auto n = parent.append_child(name);
		n.append_attribute("x") = v.x;
		n.append_attribute("y") = v.y;
		n.append_attribute("z") = v.z;
		return n;
	}

	pugi::xml_node writeQuat(pugi::xml_node parent, const char* name, const Quaternion& q)
	{
		auto n = parent.append_child(name);
		n.append_attribute("x") = q.x;
		n.append_attribute("y") = q.y;
		n.append_attribute("z") = q.z;
		n.append_attribute("w") = q.w;
		return n;
	}

	void readVec3(pugi::xml_node parent, const char* name, Vector3& v)
	{
		auto n = parent.child(name);
		v.x = n.attribute("x").as_float();
		v.y = n.attribute("y").as_float();
		v.z = n.attribute("z").as_float();
	}

	void readQuat(pugi::xml_node parent, const char* name, Quaternion& q)
	{
		auto n = parent.child(name);
		q.x = n.attribute("x").as_float();
		q.y = n.attribute("y").as_float();
		q.z = n.attribute("z").as_float();
		q.w = n.attribute("w").as_float();
	}
}


bool SceneSnapshot::save(const std::string& path) const
{
    pugi::xml_document doc;
    auto root = doc.append_child("Scene");

    if (camera)
    {
        auto camNode = root.append_child("Camera");
        writeVec3(camNode, "Position", camera->position);
        writeVec3(camNode, "Direction", camera->direction);
    }

    auto envNode = root.append_child("Environment");
    auto tod = envNode.append_child("TimeOfDay");
    tod.append_attribute("value") = environment.timeOfDay;

    for (auto& resource : resources)
    {
        auto resNode = root.append_child("Resource");
        resNode.append_attribute("file") = resource.file.c_str();

        for (auto& entity : resource.entities)
        {
            auto entNode = resNode.append_child("Entity");
            entNode.append_attribute("name") = entity.name.c_str();

            writeVec3(entNode, "Position", entity.tr.position);
            writeQuat(entNode, "Orientation", entity.tr.orientation);
            writeVec3(entNode, "Scale", entity.tr.scale);
        }
    }

    return doc.save_file(path.c_str());
}

bool SceneSnapshot::load(const std::string& path)
{
    pugi::xml_document doc;
    if (!doc.load_file(path.c_str()))
        return false;

    auto root = doc.child("Scene");
    if (!root)
        return false;

    // Camera
    if (auto camNode = root.child("Camera"))
    {
        Camera cam;
        readVec3(camNode, "Position", cam.position);
        readVec3(camNode, "Direction", cam.direction);
        camera = cam;
    }
    else
    {
        camera.reset();
    }

    // Environment
    if (auto envNode = root.child("Environment"))
    {
        environment.timeOfDay = envNode.child("TimeOfDay").attribute("value").as_float();
    }

    // Resources
    resources.clear();
    for (auto resNode : root.children("Resource"))
    {
        Resource resource;
        resource.file = resNode.attribute("file").as_string();

        for (auto entNode : resNode.children("Entity"))
        {
            Entity entity;
            entity.name = entNode.attribute("name").as_string();

            readVec3(entNode, "Position", entity.tr.position);
            readQuat(entNode, "Orientation", entity.tr.orientation);
            readVec3(entNode, "Scale", entity.tr.scale);

            resource.entities.push_back(std::move(entity));
        }

        resources.push_back(std::move(resource));
    }

    return true;
}
