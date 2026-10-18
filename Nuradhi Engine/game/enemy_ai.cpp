#include "game/enemy_ai.hpp"
#include "game/game_components.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/physics.hpp"
#include "engine/sprite/sprite.hpp"

//std
#include <cmath>
#include <random>

namespace game {

	using namespace nrd::ai;
	using nrd::ecs::Entity;

	namespace {
		//distance from the target this fighter tries to stand at, along x
		constexpr float STANDING_DISTANCE = 0.45f;

		struct Fighter {
			nrd::Transform& transform;
			nrd::RigidBody& body;
			EnemyFighter& settings;
		};

		Fighter fighter(Context& context)
		{
			return { context.registry.get<nrd::Transform>(context.self),
				context.registry.get<nrd::RigidBody>(context.self),
				context.registry.get<EnemyFighter>(context.self) };
		}

		//the target's position, or nothing if it's gone
		const nrd::Transform* target(Context& context)
		{
			const EnemyFighter& settings = context.registry.get<EnemyFighter>(context.self);
			if (!context.registry.valid(settings.target)) return nullptr;
			return context.registry.tryGet<nrd::Transform>(settings.target);
		}

		void stop(nrd::RigidBody& body)
		{
			body.velocity.x = 0.f;
			body.velocity.z = 0.f;
		}

		//--- leaves ---

		bool cannotAct(Context& context)
		{
			const Health* health = context.registry.tryGet<Health>(context.self);
			return health && (health->stunned() || health->defeated());
		}

		bool hasNoTarget(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return true;
			const Fighter self = fighter(context);
			const Health* targetHealth = context.registry.tryGet<Health>(self.settings.target);
			if (targetHealth && targetHealth->defeated()) return true;
			return glm::length(other->position - self.transform.position) > self.settings.aggroRange;
		}

		bool targetInReach(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return false;
			const Fighter self = fighter(context);
			const glm::vec3 offset = other->position - self.transform.position;
			return self.body.grounded && std::abs(offset.x) <= self.settings.attackRange && std::abs(offset.z) <= self.settings.depthTolerance;
		}

		Status faceTarget(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return Status::Failure;
			context.blackboard.set("recovered", 0.f); //a new attack routine: forget any recovery a hit cut short
			if (nrd::Sprite* sprite = context.registry.tryGet<nrd::Sprite>(context.self)) {
				sprite->flipX = other->position.x < fighter(context).transform.position.x;
			}
			stop(fighter(context).body);
			return Status::Success;
		}

		Status startAttack(Context& context)
		{
			static std::mt19937 random{ 1234u }; //fixed seed: the same fight plays out the same way each run
			Combat* combat = context.registry.tryGet<Combat>(context.self);
			if (!combat || combat->attacking()) return Status::Failure;
			const float roll = std::uniform_real_distribution<float>{ 0.f, 1.f }(random);
			combat->start(roll < fighter(context).settings.kickChance ? "kick" : "punch");
			return Status::Success;
		}

		Status waitForAttackToFinish(Context& context)
		{
			const Combat* combat = context.registry.tryGet<Combat>(context.self);
			return combat && combat->attacking() ? Status::Running : Status::Success;
		}

		Status approachTarget(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return Status::Failure;
			Fighter self = fighter(context);

			//stand off to the side we're already on, level with the target in depth
			const float side = self.transform.position.x < other->position.x ? -1.f : 1.f;
			const glm::vec3 goal{ other->position.x + side * STANDING_DISTANCE, self.transform.position.y, other->position.z };
			const glm::vec3 offset = goal - self.transform.position;
			const float speed = self.settings.moveSpeed;
			self.body.velocity.x = std::abs(offset.x) > 0.05f ? std::copysign(speed, offset.x) : 0.f;
			self.body.velocity.z = std::abs(offset.z) > self.settings.depthTolerance * 0.5f ? std::copysign(speed, offset.z) : 0.f;
			return Status::Running;
		}

		Status idle(Context& context)
		{
			stop(fighter(context).body);
			return Status::Running;
		}
	}

	NodePtr makeFighterTree()
	{
		return selector(
			sequence(condition(cannotAct), action([](Context&) { return Status::Running; })),
			sequence(condition(hasNoTarget), action(idle)),
			statefulSequence(
				condition(targetInReach),
				action(faceTarget),
				action(startAttack),
				action(waitForAttackToFinish),
				action([](Context& context) { stop(fighter(context).body); return Status::Success; }),
				action([](Context& context) {
					//recover: stand still for recoverTime (a Wait node with a per-entity duration)
					const float waited = context.blackboard.get<float>("recovered", 0.f) + context.dt;
					if (waited < fighter(context).settings.recoverTime) {
						context.blackboard.set("recovered", waited);
						return Status::Running;
					}
					context.blackboard.set("recovered", 0.f);
					return Status::Success;
				})),
			action(approachTarget));
	}
}
