#include "physics.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <glm/gtc/quaternion.hpp>

#include <engine.h>
#include <core/log.h>

static void TraceImpl(const char* inFMT, ...)
{
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") "
		<< (inMessage != nullptr ? inMessage : "") << std::endl;
	return true;
}
#endif

ruya::Physics::Physics()
{
	JPH::RegisterDefaultAllocator();

	JPH::Trace = TraceImpl;

#ifdef JPH_ENABLE_ASSERTS
	JPH::JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)
#endif

		JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(32 * 1024 * 1024);
	job_system = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

	constexpr JPH::uint cMaxBodies = 65536;
	constexpr JPH::uint cNumBodyMutexes = 0;
	constexpr JPH::uint cMaxBodyPairs = 65536;
	constexpr JPH::uint cMaxContactConstraints = 10240;

	physics_system.Init(
		cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
		broad_phase_layer_interface,
		object_vs_broadphase_layer_filter,
		object_vs_object_layer_filter);

	JPH::PhysicsSettings settings;
	settings.mPenetrationSlop = 0.005f;
	settings.mSpeculativeContactDistance = 0.02f;
	settings.mBaumgarte = 0.2f;
	settings.mNumVelocitySteps = 10;
	settings.mNumPositionSteps = 3;
	physics_system.SetPhysicsSettings(settings);

	physics_system.SetBodyActivationListener(&body_activation_listener);
	physics_system.SetContactListener(&contact_listener);

	joltDebugRenderer = std::make_unique<ruya::JoltDebugRenderer>();

	drawDebugLines = std::make_unique<bool>();
	*drawDebugLines = false;

	riggedBodyRyIDCounter = RyID(0);

	RUYA_LOG_INFO("[Physics] Physics instance created");
}

ruya::Physics::~Physics()
{
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
	JPH::BodyIDVector   body_ids;
	physics_system.GetBodies(body_ids);

	for (const JPH::BodyID& id : body_ids)
	{
		body_interface.RemoveBody(id);
		body_interface.DestroyBody(id);
	}

	JPH::UnregisterTypes();

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

	RUYA_LOG_INFO("[Physics] Physics instance destroyed");
}

void ruya::Physics::DebugDrawRiggedBodies()
{
	if (!(*drawDebugLines)) return;

	JPH::BodyManager::DrawSettings draw_settings;
	draw_settings.mDrawBoundingBox = false;
	draw_settings.mDrawCenterOfMassTransform = false;
	draw_settings.mDrawShape = true;
	draw_settings.mDrawShapeWireframe = true;

	physics_system.DrawBodies(draw_settings, joltDebugRenderer.get());
}

void ruya::Physics::StartSimulation()
{
	physicsStep = 0;
	accumulator = 0.0f;

	physics_system.OptimizeBroadPhase();
}

void ruya::Physics::UpdateSimulation()
{
	float deltaTime = engine->GetWindow()->GetDeltaTime();

	constexpr float cMaxAccumulated = 0.25f;
	accumulator += deltaTime;
	if (accumulator > cMaxAccumulated)
		accumulator = cMaxAccumulated;

	constexpr int cCollisionSteps = 2;

	while (accumulator >= cFixedDeltaTime)
	{
		physics_system.Update(cFixedDeltaTime, cCollisionSteps, temp_allocator.get(), job_system.get());
		accumulator -= cFixedDeltaTime;
		++physicsStep;
	}
}

void ruya::Physics::ShutdownSimulation()
{
}

ruya::RyID ruya::Physics::CreateRiggedBody(glm::vec3 halfWidthHeightDepth, glm::vec3 position, glm::quat rotation, PhysicsMotionType physicsMotionType)
{
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	JPH::BoxShapeSettings box_shape_settings(JPH::Vec3(halfWidthHeightDepth.x, halfWidthHeightDepth.y, halfWidthHeightDepth.z));
	box_shape_settings.SetEmbedded();

	JPH::ShapeSettings::ShapeResult box_shape_result = box_shape_settings.Create();
	JPH::ShapeRefC box_shape = box_shape_result.Get();

	JPH::Quat jolt_rotation(rotation.x, rotation.y, rotation.z, rotation.w);

	JPH::EMotionType jolt_motion_type;
	JPH::ObjectLayer jolt_layer;

	switch (physicsMotionType)
	{
	case PhysicsMotionType::Static:
		jolt_motion_type = JPH::EMotionType::Static;
		jolt_layer = Layers::NON_MOVING;
		break;
	case PhysicsMotionType::Kinematic:
		jolt_motion_type = JPH::EMotionType::Kinematic;
		jolt_layer = Layers::MOVING;
		break;
	case PhysicsMotionType::Dynamic:
	default:
		jolt_motion_type = JPH::EMotionType::Dynamic;
		jolt_layer = Layers::MOVING;
		break;
	}

	JPH::BodyCreationSettings body_settings(
		box_shape,
		JPH::RVec3(position.x, position.y, position.z),
		jolt_rotation,
		jolt_motion_type,
		jolt_layer
	);

	JPH::Body* body = body_interface.CreateBody(body_settings);

	if (body == nullptr)
	{
		RUYA_LOG_ERROR("[Physics] Failed to create rigged body!");
		return RyID();
	}

	JPH::EActivation activation = (jolt_motion_type == JPH::EMotionType::Static)
		? JPH::EActivation::DontActivate
		: JPH::EActivation::Activate;

	body_interface.AddBody(body->GetID(), activation);

	RyID ryID;

	if (availableRiggedBodyRyIDs.empty())
	{
		ryID = riggedBodyRyIDCounter;
		riggedBodyRyIDCounter = RyID(ryID.GetRawID() + 1);
	}
	else
	{
		ryID = availableRiggedBodyRyIDs.front();
		availableRiggedBodyRyIDs.pop();
	}

	ryID2JPHBodyID.insert({ ryID, body->GetID() });

	return ryID;
}

void ruya::Physics::DestroyRiggedBody(RyID riggedBodyRyID)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end())
	{
		return;
	}

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	body_interface.RemoveBody(jolt_body_id);
	body_interface.DestroyBody(jolt_body_id);

	ryID2JPHBodyID.erase(it);

	availableRiggedBodyRyIDs.push(riggedBodyRyID);
}

void ruya::Physics::SetRiggedBodyMotionType(RyID riggedBodyRyID, PhysicsMotionType physicsMotionType)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID old_jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	JPH::RVec3 position;
	JPH::Quat rotation;
	body_interface.GetPositionAndRotation(old_jolt_body_id, position, rotation);

	JPH::ShapeRefC shape = body_interface.GetShape(old_jolt_body_id);
	float friction = body_interface.GetFriction(old_jolt_body_id);
	float restitution = body_interface.GetRestitution(old_jolt_body_id);

	body_interface.RemoveBody(old_jolt_body_id);
	body_interface.DestroyBody(old_jolt_body_id);

	JPH::EMotionType jolt_motion_type;
	JPH::ObjectLayer jolt_layer;

	switch (physicsMotionType)
	{
	case PhysicsMotionType::Static:
		jolt_motion_type = JPH::EMotionType::Static;
		jolt_layer = Layers::NON_MOVING;
		break;
	case PhysicsMotionType::Kinematic:
		jolt_motion_type = JPH::EMotionType::Kinematic;
		jolt_layer = Layers::MOVING;
		break;
	case PhysicsMotionType::Dynamic:
	default:
		jolt_motion_type = JPH::EMotionType::Dynamic;
		jolt_layer = Layers::MOVING;
		break;
	}

	JPH::BodyCreationSettings body_settings(
		shape,
		position,
		rotation,
		jolt_motion_type,
		jolt_layer
	);

	body_settings.mFriction = friction;
	body_settings.mRestitution = restitution;

	JPH::Body* new_body = body_interface.CreateBody(body_settings);

	if (new_body != nullptr)
	{
		JPH::EActivation activation = (jolt_motion_type == JPH::EMotionType::Static)
			? JPH::EActivation::DontActivate
			: JPH::EActivation::Activate;

		body_interface.AddBody(new_body->GetID(), activation);

		it->second = new_body->GetID();
	}
	else
	{
		RUYA_LOG_ERROR("[Physics] Failed to change Motion Type!");
		ryID2JPHBodyID.erase(it);
	}
}

void ruya::Physics::SetRiggedBodyPosition(RyID riggedBodyRyID, glm::vec3 position)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	body_interface.SetPosition(
		jolt_body_id,
		JPH::RVec3(position.x, position.y, position.z),
		JPH::EActivation::Activate
	);
}

void ruya::Physics::SetRiggedBodyRotation(RyID riggedBodyRyID, glm::quat rotation)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	JPH::Quat jolt_rotation(rotation.x, rotation.y, rotation.z, rotation.w);

	body_interface.SetRotation(
		jolt_body_id,
		jolt_rotation,
		JPH::EActivation::Activate
	);
}

void ruya::Physics::SetRiggedBodyPositionAndRotation(RyID riggedBodyRyID, glm::vec3 position, glm::quat rotation)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	JPH::Quat jolt_rotation(rotation.x, rotation.y, rotation.z, rotation.w);
	JPH::RVec3 jolt_position(position.x, position.y, position.z);

	body_interface.SetPositionAndRotation(
		jolt_body_id,
		jolt_position,
		jolt_rotation,
		JPH::EActivation::Activate
	);
}

void ruya::Physics::SetRiggedBodySize(RyID riggedBodyRyID, glm::vec3 halfWidthHeightDepth)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	JPH::BoxShapeSettings box_shape_settings(JPH::Vec3(halfWidthHeightDepth.x, halfWidthHeightDepth.y, halfWidthHeightDepth.z));
	box_shape_settings.SetEmbedded();

	JPH::ShapeSettings::ShapeResult box_shape_result = box_shape_settings.Create();
	JPH::ShapeRefC new_box_shape = box_shape_result.Get();

	body_interface.SetShape(jolt_body_id, new_box_shape, true, JPH::EActivation::Activate);
}

void ruya::Physics::GetRiggedBodyPositionAndRotation(RyID riggedBodyRyID, glm::vec3& outPosition, glm::quat& outRotationQuat)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	JPH::RVec3 jolt_pos;
	JPH::Quat jolt_rot;
	body_interface.GetPositionAndRotation(jolt_body_id, jolt_pos, jolt_rot);

	outPosition = glm::vec3(jolt_pos.GetX(), jolt_pos.GetY(), jolt_pos.GetZ());

	outRotationQuat = glm::quat(jolt_rot.GetW(), jolt_rot.GetX(), jolt_rot.GetY(), jolt_rot.GetZ());
}

void ruya::Physics::ResetRigidBodyVelocity(RyID riggedBodyRyID)
{
	auto it = ryID2JPHBodyID.find(riggedBodyRyID);
	if (it == ryID2JPHBodyID.end()) return;

	JPH::BodyID jolt_body_id = it->second;
	JPH::BodyInterface& body_interface = physics_system.GetBodyInterfaceNoLock();

	body_interface.SetLinearAndAngularVelocity(
		jolt_body_id,
		JPH::Vec3::sZero(),
		JPH::Vec3::sZero()
	);
}