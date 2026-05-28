#pragma once

#include "Physics/JoltHeader.h"
#include "RenderObject/SplineSweep/ShapeProfile.h"
#include "RenderObject/SplineSweep/Spline.h"
#include "RenderObject/SplineSweep/SplineSweepMesh.h"
#include "Scene/RenderObject.h"

#include <Jolt/Physics/Body/BodyID.h>

#include <optional>
#include <string>

namespace DirectX
{
	class ResourceUploadBatch;
}

struct GraphicsResources;
class PhysicsManager;
class RenderEntity;
class RenderSystem;
class RenderWorld;

struct SplineConstructionCreateParams
{
	std::string materialName = "General";
	ObjectTransformation transformation{};
	bool createRenderEntity = true;
	bool createMeshPhysics = true;
};

class SplineConstruction
{
public:
	Spline spline;
	ShapeProfile2D profile;
	SplineSweepMeshSettings sweepSettings;
	SplineSweepMesh mesh;
	VertexBufferModel model;
	RenderEntity* entity{};
	std::optional<JPH::BodyID> physicsBody;

	void regenerate(RenderSystem& renderSystem, GraphicsResources& resources, RenderWorld& renderWorld, PhysicsManager* physicsManager, DirectX::ResourceUploadBatch& uploadBatch, const SplineConstructionCreateParams& params = {});
	void removeFromWorld(RenderWorld& renderWorld, PhysicsManager* physicsManager);
};