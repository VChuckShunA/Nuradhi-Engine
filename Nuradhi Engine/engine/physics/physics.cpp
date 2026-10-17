#include "engine/physics/physics.hpp"
#include "engine/core/components.hpp"

//libs
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

//std
#include <algorithm>
#include <iostream>
#include <vector>

namespace nrd {

	namespace {
		constexpr int MAX_RESOLVE_PASSES = 4; //a later push can move a body back into an earlier obstacle

		void applyGravityAndMove(ecs::Registry& registry, float dt)
		{
			registry.each<Transform, RigidBody>([dt](ecs::Entity, Transform& transform, RigidBody& body) {
				if (body.gravity != 0.f) {
					body.velocity.y = std::min(body.velocity.y + body.gravity * dt, body.terminalVelocity);
				}
				transform.position += body.velocity * dt;
			});
		}

		void resolveCollisions(ecs::Registry& registry, bool logContacts)
		{
			//static colliders don't move during the step, so their bounds are gathered once
			std::vector<Aabb> obstacles;
			registry.each<Transform, Collider>([&](ecs::Entity, Transform& transform, Collider& collider) {
				if (collider.isStatic) obstacles.push_back(collider.worldBounds(transform.position));
			});

			registry.each<Transform, RigidBody, Collider>([&](ecs::Entity, Transform& transform, RigidBody& body, Collider& collider) {
				if (collider.isStatic) return;

				body.grounded = body.hitCeiling = body.hitWall = false;
				int contactCount = 0;
				for (int pass = 0; pass < MAX_RESOLVE_PASSES; pass++) {
					bool moved = false;
					for (const Aabb& obstacle : obstacles) {
						const auto contact = computeContact(collider.worldBounds(transform.position), obstacle);
						if (!contact) continue;

						transform.position += contact->normal * contact->depth;
						//stop moving into the obstacle; sliding along it is fine
						const float intoObstacle = glm::dot(body.velocity, contact->normal);
						if (intoObstacle < 0.f) body.velocity -= contact->normal * intoObstacle;

						moved = true;
						contactCount++;
						//-y is up, so being pushed towards -y means we're standing on the obstacle
						if (contact->normal.y < 0.f) body.grounded = true;
						else if (contact->normal.y > 0.f) body.hitCeiling = true;
						else body.hitWall = true;
					}
					if (!moved) break;
				}

				if (logContacts && contactCount > 0) {
					std::cout << "contacts " << contactCount
						<< " grounded " << body.grounded
						<< " ceiling " << body.hitCeiling
						<< " wall " << body.hitWall
						<< " position " << glm::to_string(transform.position) << "\n";
				}
			});
		}
	}

	void stepPhysics(ecs::Registry& registry, float dt, const PhysicsSettings& settings, bool logContacts)
	{
		dt = std::min(dt, settings.maxStep);
		applyGravityAndMove(registry, dt);
		resolveCollisions(registry, logContacts);
	}
}
