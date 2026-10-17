#pragma once

#include "engine/ecs/registry.hpp"
#include "engine/physics/collision.hpp"

//libs
#include <glm/glm.hpp>

namespace nrd {

	//Makes an entity move under its own velocity and gravity. With a non-static Collider it is also pushed out of
	//static colliders. Units are world units and seconds; +y is down, so gravity is positive.
	struct RigidBody {
		glm::vec3 velocity{ 0.f };
		float gravity = 9.8f; //0 for things that float
		float terminalVelocity = 5.f; //fastest fall speed

		//contacts from the latest physics step
		bool grounded = false; //standing on something
		bool hitCeiling = false;
		bool hitWall = false;
	};

	struct PhysicsSettings {
		//longest step simulated at once: a long frame (window drag, breakpoint) would otherwise carry a body through thin floors
		float maxStep = 0.05f;
	};

	//Gravity, then movement, then collision, for every entity with a Transform and RigidBody.
	//With logContacts it prints each dynamic body's contacts and position every step.
	void stepPhysics(ecs::Registry& registry, float dt, const PhysicsSettings& settings, bool logContacts);
}
