#include "game/enemy_ai.hpp"
#include "game/game_components.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/physics.hpp"
#include "engine/sprite/sprite.hpp"

//std
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

namespace game {

	using namespace nrd::ai;
	using nrd::ecs::Entity;

	namespace {
		//fixed seeds: the same fight plays out the same way each run
		std::mt19937 planRandom{ 1234u };
		std::mt19937 squadRandom{ 4321u };

		float roll(std::mt19937& random) { return std::uniform_real_distribution<float>{ 0.f, 1.f }(random); }

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

		//the sheets are drawn facing right
		void faceTowards(Context& context, const nrd::Transform& other)
		{
			if (nrd::Sprite* sprite = context.registry.tryGet<nrd::Sprite>(context.self)) {
				sprite->flipX = other.position.x < fighter(context).transform.position.x;
			}
		}

		//A push away from other enemies standing too close, so a crowd spreads out instead of stacking up. Depth counts
		//for less than sideways distance: two enemies a lane apart still overlap on screen. An attacker only gives way to
		//other attackers: the waiters in its path are the ones who step aside, pushed harder by an attacker coming at them.
		glm::vec2 separation(Context& context, float radius)
		{
			const nrd::Transform& self = context.registry.get<nrd::Transform>(context.self);
			const bool selfAttacks = context.registry.get<EnemyFighter>(context.self).role == EnemyFighter::Role::Attacker;
			glm::vec2 push{ 0.f };
			context.registry.each<EnemyFighter, nrd::Transform>([&](Entity other, EnemyFighter& otherFighter, nrd::Transform& transform) {
				if (other == context.self) return;
				const bool otherAttacks = otherFighter.role == EnemyFighter::Role::Attacker;
				if (selfAttacks && !otherAttacks) return;
				glm::vec2 away{ self.position.x - transform.position.x, (self.position.z - transform.position.z) * 0.6f };
				const float distance = glm::length(away);
				if (distance >= radius) return;
				if (distance < 1e-3f) away = { (context.self % 2 == 0) ? 1.f : -1.f, 0.4f }; //exactly on top: split by who is who
				push += glm::normalize(away) * (1.f - distance / radius) * (!selfAttacks && otherAttacks ? 1.8f : 1.f);
			});
			return push;
		}

		//Walks towards `goal` (x and z), slowing down over the last bit so it doesn't jitter, with `crowding` pushing it
		//clear of other enemies. It walks at `speed` when close and hurries up to `hurrySpeed` when the goal is far off,
		//so a long way round is run rather than strolled. Sets the velocity; returns how far from the goal it still is.
		float walkTo(Context& context, const glm::vec2& goal, float speed, float hurrySpeed, float crowding)
		{
			Fighter self = fighter(context);
			const glm::vec2 offset = goal - glm::vec2{ self.transform.position.x, self.transform.position.z };
			speed = glm::mix(speed, hurrySpeed, glm::clamp((glm::length(offset) - 0.35f) / 0.6f, 0.f, 1.f));
			glm::vec2 velocity = glm::clamp(offset / 0.15f, glm::vec2{ -1.f }, glm::vec2{ 1.f }) * speed; //full speed until the last 0.15
			velocity += separation(context, 0.55f) * speed * crowding;
			const float length = glm::length(velocity);
			if (length > speed) velocity *= speed / length;
			if (glm::length(offset) < 0.04f && length < speed * 0.15f) velocity = glm::vec2{ 0.f }; //arrived
			self.body.velocity.x = velocity.x;
			self.body.velocity.z = velocity.y;
			return glm::length(offset);
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

		bool isAttacker(Context& context)
		{
			return fighter(context).settings.role == EnemyFighter::Role::Attacker;
		}

		bool targetInReach(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return false;
			const Fighter self = fighter(context);
			//a target in the air can't be hit by anything on the ground: wait for it to come down
			const nrd::RigidBody* targetBody = context.registry.tryGet<nrd::RigidBody>(self.settings.target);
			if (targetBody && !targetBody->grounded) return false;
			const glm::vec3 offset = other->position - self.transform.position;
			return self.body.grounded && std::abs(offset.x) <= self.settings.engageDistance + 0.08f && std::abs(offset.z) <= self.settings.depthTolerance;
		}

		//starts a new attack routine: face the target, stand still
		Status beginAttack(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return Status::Failure;
			context.blackboard.set("recovered", 0.f); //forget any recovery or wind-up a hit cut short
			context.blackboard.set("windup", 0.f);
			faceTowards(context, *other);
			stop(fighter(context).body);
			return Status::Success;
		}

		//the tell: it stops and squares up to the target for a moment before swinging, turning with it if it moves
		Status windUp(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return Status::Failure;
			Fighter self = fighter(context);
			faceTowards(context, *other);
			stop(self.body);
			const float waited = context.blackboard.get<float>("windup", 0.f) + context.dt;
			if (waited < self.settings.windup) {
				context.blackboard.set("windup", waited);
				return Status::Running;
			}
			context.blackboard.set("windup", 0.f);
			return Status::Success;
		}

		Status startAttack(Context& context)
		{
			const nrd::Transform* other = target(context);
			Combat* combat = context.registry.tryGet<Combat>(context.self);
			if (!other || !combat || combat->attacking()) return Status::Failure;
			const Fighter self = fighter(context);

			//the target moved while it was winding up: out of reach, so don't swing at the air; the tree walks up again
			const glm::vec3 offset = other->position - self.transform.position;
			const float reach = self.settings.plannedKick ? self.settings.kickReach : self.settings.punchReach;
			if (std::abs(offset.x) > reach + 0.12f || std::abs(offset.z) > self.settings.depthTolerance * 2.f) return Status::Failure;

			faceTowards(context, *other);
			combat->start(self.settings.plannedKick ? "kick" : "punch");
			return Status::Success;
		}

		Status waitForAttackToFinish(Context& context)
		{
			const Combat* combat = context.registry.tryGet<Combat>(context.self);
			return combat && combat->attacking() ? Status::Running : Status::Success;
		}

		//stands still for recoverTime (a Wait node with a per-entity duration)
		Status recover(Context& context)
		{
			stop(fighter(context).body);
			const float waited = context.blackboard.get<float>("recovered", 0.f) + context.dt;
			if (waited < fighter(context).settings.recoverTime) {
				context.blackboard.set("recovered", waited);
				return Status::Running;
			}
			context.blackboard.set("recovered", 0.f);
			return Status::Success;
		}

		//the turn is over: say so (the squad takes it back), and decide how the next attack will go
		Status finishTurn(Context& context)
		{
			EnemyFighter& settings = fighter(context).settings;
			settings.attackDone = true;
			planNextAttack(settings);
			return Status::Success;
		}

		//walks up to the target on the side it's already on, and lines up with it in depth
		Status approachTarget(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return Status::Failure;
			Fighter self = fighter(context);
			faceTowards(context, *other);
			const float side = self.transform.position.x < other->position.x ? -1.f : 1.f;
			//a long way off it runs in (Blaze has a run animation for that), then walks the last stretch
			walkTo(context, { other->position.x + side * self.settings.engageDistance, other->position.z }, self.settings.moveSpeed, self.settings.moveSpeed * 3.f, 0.5f);
			return Status::Running;
		}

		//waits at its slot around the target, facing it, backing off if the target comes close
		Status holdSlot(Context& context)
		{
			const nrd::Transform* other = target(context);
			if (!other) return Status::Failure;
			Fighter self = fighter(context);
			faceTowards(context, *other);
			walkTo(context, self.settings.slot, self.settings.moveSpeed * 0.8f, self.settings.moveSpeed * 2.f, 0.8f); //keeps up when the target moves
			return Status::Running;
		}

		Status idle(Context& context)
		{
			stop(fighter(context).body);
			return Status::Running;
		}
	}

	void planNextAttack(EnemyFighter& fighter)
	{
		fighter.plannedKick = roll(planRandom) < fighter.kickChance;
		//a kick reaches further, so it's thrown from further away
		fighter.engageDistance = fighter.plannedKick ? 0.72f + 0.14f * roll(planRandom) : 0.5f + 0.1f * roll(planRandom);
	}

	NodePtr makeFighterTree()
	{
		return selector(
			sequence(condition(cannotAct), action([](Context&) { return Status::Running; })),
			sequence(condition(hasNoTarget), action(idle)),
			sequence(
				condition(isAttacker),
				selector(
					statefulSequence(
						condition(targetInReach),
						action(beginAttack),
						action(windUp),
						action(startAttack),
						action(waitForAttackToFinish),
						action(recover),
						action(finishTurn)),
					action(approachTarget))),
			action(holdSlot));
	}

	void enemySquadSystem(nrd::Engine& engine, float dt)
	{
		nrd::ecs::Registry& registry = engine.registry();
		const Entity squadEntity = registry.findFirst<EnemySquad>();
		if (squadEntity == nrd::ecs::NullEntity) return;
		EnemySquad& squad = registry.get<EnemySquad>(squadEntity);
		squad.time += dt;
		squad.gapTimer = std::max(0.f, squad.gapTimer - dt);

		struct Member {
			Entity entity;
			EnemyFighter* fighter;
			nrd::Transform* transform;
			Health* health;
		};
		//enemies that are still up, grouped by who they're fighting
		std::unordered_map<Entity, std::vector<Member>> groups;
		registry.each<EnemyFighter, nrd::Transform, Health>([&](Entity entity, EnemyFighter& enemy, nrd::Transform& transform, Health& health) {
			if (!health.defeated()) groups[enemy.target].push_back({ entity, &enemy, &transform, &health });
		});

		static int lastReport = -1;
		const bool report = engine.debugLog && static_cast<int>(squad.time / 3.f) != lastReport;
		if (report) lastReport = static_cast<int>(squad.time / 3.f);
		for (auto& [targetEntity, members] : groups) {
			if (!registry.valid(targetEntity)) continue;
			const nrd::Transform* targetTransform = registry.tryGet<nrd::Transform>(targetEntity);
			if (!targetTransform) continue;
			const glm::vec3 targetPosition = targetTransform->position;
			const auto sideOf = [&](const Member& member) { return member.transform->position.x < targetPosition.x ? -1 : 1; };
			std::sort(members.begin(), members.end(), [](const Member& a, const Member& b) { return a.entity < b.entity; }); //stable order: slots don't shuffle

			//1. time passes; an attacker whose turn is over, who was hit, or who can't get at the target gives its turn back
			int attackers = 0;
			for (Member& member : members) {
				EnemyFighter& enemy = *member.fighter;
				if (enemy.role == EnemyFighter::Role::Attacker) {
					enemy.turnTime += dt;
					if (engine.debugLog) {
						const Combat* combat = registry.tryGet<Combat>(member.entity);
						const bool swinging = combat && combat->attacking();
						if (swinging && !enemy.swinging) std::cout << "squad t=" << squad.time << ": " << member.entity << " swings (" << combat->currentMove << ") after " << enemy.turnTime << " s\n";
						enemy.swinging = swinging;
					}
					const bool hit = member.health->stunned();
					if (enemy.attackDone || hit || enemy.turnTime > 8.f) {
						if (engine.debugLog) std::cout << "squad t=" << squad.time << ": " << member.entity << (enemy.attackDone ? " finished its turn" : hit ? " was hit" : " gave up reaching the target") << "\n";
						enemy.role = EnemyFighter::Role::Waiter;
						enemy.waited = 0.f;
						enemy.turnTime = 0.f;
						enemy.attackDone = false;
						//one that was hit stays back a shorter while: it's been hurt, not tired
						enemy.cooldown = hit ? 0.4f + 0.5f * roll(squadRandom)
							: squad.minCooldown + (squad.maxCooldown - squad.minCooldown) * roll(squadRandom);
					}
				}
				else {
					enemy.waited += dt;
					enemy.cooldown = std::max(0.f, enemy.cooldown - dt);
				}
				if (enemy.role == EnemyFighter::Role::Attacker) attackers++;
			}

			//2. send one more in, if there's room: the one that has waited longest, and from the side nobody is attacking from
			if (attackers < squad.maxAttackers && squad.gapTimer <= 0.f) {
				Member* best = nullptr;
				float bestScore = -1e9f;
				for (Member& member : members) {
					EnemyFighter& enemy = *member.fighter;
					if (enemy.role != EnemyFighter::Role::Waiter || enemy.cooldown > 0.f || member.health->stunned()) continue;
					const float distance = glm::length(member.transform->position - targetPosition);
					if (distance > enemy.aggroRange) continue;
					float score = enemy.waited - 0.2f * distance + 0.3f * roll(squadRandom);
					for (const Member& other : members) {
						if (other.fighter->role != EnemyFighter::Role::Attacker) continue;
						score += sideOf(other) == sideOf(member) ? -1.f : 0.8f;
					}
					if (score > bestScore) {
						bestScore = score;
						best = &member;
					}
				}
				if (best) {
					if (engine.debugLog) std::cout << "squad t=" << squad.time << ": " << best->entity << " sent in (" << attackers + 1 << " attacking, " << (sideOf(*best) < 0 ? "left" : "right") << ")\n";
					best->fighter->role = EnemyFighter::Role::Attacker;
					best->fighter->turnTime = 0.f;
					best->fighter->attackDone = false;
					squad.gapTimer = squad.attackGap;
				}
			}

			//3. the waiters take their places: fanned out on each side of the target, the first ones nearest, each in its own
			//lane of depth, drifting a little so they don't stand like posts
			//The attackers walk in along the target's own depth (lane 0), so the waiters keep to the lanes either side of it.
			constexpr float LANES[4] = { 0.5f, -0.5f, 0.95f, -0.95f };
			for (const int side : { -1, 1 }) {
				int index = 0;
				for (Member& member : members) {
					EnemyFighter& enemy = *member.fighter;
					if (enemy.role != EnemyFighter::Role::Waiter || sideOf(member) != side) continue;
					const float sway = static_cast<float>(member.entity);
					enemy.slot.x = targetPosition.x + side * (squad.standoff + index * squad.slotSpacing) + 0.12f * std::sin(squad.time * 0.8f + sway * 1.7f);
					enemy.slot.y = std::clamp(targetPosition.z + LANES[(index + (side > 0 ? 1 : 0)) % 4] + 0.08f * std::cos(squad.time * 0.6f + sway), squad.depthRange.x, squad.depthRange.y);
					index++;
				}
			}
			if (report) {
				std::cout << "squad t=" << squad.time << " target at x " << targetPosition.x << " z " << targetPosition.z << "\n";
				for (const Member& member : members) {
					const EnemyFighter& enemy = *member.fighter;
					std::cout << "   " << member.entity << (enemy.role == EnemyFighter::Role::Attacker ? " attacker" : " waiter  ") << " at x " << member.transform->position.x << " z " << member.transform->position.z
						<< "  slot x " << enemy.slot.x << " z " << enemy.slot.y << "\n";
				}
			}
		}
	}
}
