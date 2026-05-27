#pragma once
#include <iostream>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <queue>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <core/ry_id.h>

#include "jolt_debug_renderer.h"

namespace Layers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}


namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr JPH::uint            NUM_LAYERS(2);
}

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
		case Layers::MOVING:     return true;
		default:                 JPH_ASSERT(false); return false;
		}
	}
};


class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	BPLayerInterfaceImpl()
	{
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual JPH::uint             GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
	virtual JPH::BroadPhaseLayer  GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
		default: JPH_ASSERT(false); return "INVALID";
		}
	}
#endif

private:
	JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:     return true;
		default:                 JPH_ASSERT(false); return false;
		}
	}
};


class MyContactListener : public JPH::ContactListener
{
public:
	virtual JPH::ValidateResult OnContactValidate(
		const JPH::Body& inBody1, const JPH::Body& inBody2,
		JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override
	{
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void OnContactAdded(
		const JPH::Body& inBody1, const JPH::Body& inBody2,
		const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
	}

	virtual void OnContactPersisted(
		const JPH::Body& inBody1, const JPH::Body& inBody2,
		const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
	}

	virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {}
};


class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
	virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {}
	virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {}
};

namespace ruya
{
	enum class PhysicsMotionType
	{
		Static,
		Kinematic,
		Dynamic,
	};

	class Physics
	{
	public:
		Physics();
		~Physics();

		Physics(const Physics&) = delete;
		Physics& operator=(const Physics&) = delete;

		void DebugDrawRiggedBodies();

		void StartSimulation();
		void UpdateSimulation();
		void ShutdownSimulation();

		RyID CreateRiggedBody(glm::vec3 halfWidthHeightDepth, glm::vec3 position, glm::quat rotation, PhysicsMotionType physicsMotionType);
		void DestroyRiggedBody(RyID riggedBodyRyID);
		void SetRiggedBodyMotionType(RyID riggedBodyRyID, PhysicsMotionType physicsMotionType);
		void SetRiggedBodyPosition(RyID riggedBodyRyID, glm::vec3 position);
		void SetRiggedBodyRotation(RyID riggedBodyRyID, glm::quat rotation);
		void SetRiggedBodyPositionAndRotation(RyID riggedBodyRyID, glm::vec3 position, glm::quat rotation);
		void SetRiggedBodySize(RyID riggedBodyRyID, glm::vec3 halfWidthHeightDepth);

		void GetRiggedBodyPositionAndRotation(RyID riggedBodyRyID, glm::vec3& outPosition, glm::quat& outRotationQuat);

		void ResetRigidBodyVelocity(RyID riggedBodyRyID);

		bool* DrawDebugLines() { return drawDebugLines.get(); }

	private:
		std::unique_ptr<JPH::TempAllocatorImpl>      temp_allocator;
		std::unique_ptr<JPH::JobSystemSingleThreaded> job_system;

		BPLayerInterfaceImpl                    broad_phase_layer_interface;
		ObjectVsBroadPhaseLayerFilterImpl       object_vs_broadphase_layer_filter;
		ObjectLayerPairFilterImpl               object_vs_object_layer_filter;
		MyBodyActivationListener                body_activation_listener;
		MyContactListener                       contact_listener;

		JPH::PhysicsSystem                      physics_system;

		static constexpr float cFixedDeltaTime = 1.0f / 120.0f;
		float accumulator = 0.0f;

		JPH::uint physicsStep = 0;

		std::unordered_map<RyID, JPH::BodyID> ryID2JPHBodyID;
		RyID riggedBodyRyIDCounter;
		std::queue<RyID> availableRiggedBodyRyIDs;

		std::unique_ptr<JoltDebugRenderer> joltDebugRenderer;

		std::unique_ptr<bool> drawDebugLines;
	};
}