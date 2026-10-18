#include "game/game_systems.hpp"
#include "game/game_components.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/physics.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

//std
#include <algorithm>
#include <iostream>
#include <vector>

namespace game {

	using nrd::ecs::Entity;

	void playerAttackInputSystem(nrd::Engine& engine, float dt)
	{
		const nrd::Input& input = engine.input();
		engine.registry().each<PlayerController, Combat, nrd::RigidBody>([&](Entity, PlayerController&, Combat& combat, nrd::RigidBody& body) {
			if (combat.attacking()) return;
			if (body.grounded) {
				if (input.actionPressed("punch")) combat.start("punch");
				else if (input.actionPressed("kick")) combat.start("kick");
			}
			else if (!combat.airAttackUsed) {
				const char* move = input.actionPressed("punch") ? "jumpPunch" : (input.actionPressed("kick") ? "jumpKick" : nullptr);
				if (move) {
					combat.start(move);
					combat.airAttackUsed = true;
				}
			}
		});
	}

	void playerControlSystem(nrd::Engine& engine, float dt)
	{
		const nrd::Input& input = engine.input();
		nrd::ecs::Registry& registry = engine.registry();
		registry.each<PlayerController, nrd::RigidBody>([&](Entity entity, PlayerController& controller, nrd::RigidBody& body) {
			//committed to an attack: plant the feet, or keep flying on the jump's momentum
			const Combat* combat = registry.tryGet<Combat>(entity);
			if (combat && combat->attacking()) {
				const AttackMove* move = combat->current();
				if (!move || !move->airborne) body.velocity.x = body.velocity.z = 0.f;
				return;
			}

			//+z is forward, away from the camera
			glm::vec3 direction{ input.axis("moveX"), 0.f, input.axis("moveZ") };
			if (glm::dot(direction, direction) > 0.f) direction = glm::normalize(direction);
			body.velocity.x = direction.x * controller.moveSpeed;
			body.velocity.z = direction.z * controller.moveSpeed;

			//holding jump jumps again on landing
			if (input.actionDown("jump") && body.grounded) {
				body.velocity.y = -controller.jumpSpeed; //-y is up
				body.grounded = false;
			}
		});
	}

	void cameraYawSystem(nrd::Engine& engine, float dt)
	{
		const float turn = engine.input().axis("cameraYaw");
		engine.registry().each<CameraYawControl, nrd::Transform>([&](Entity, CameraYawControl& control, nrd::Transform& transform) {
			transform.rotation.y = glm::mod(transform.rotation.y + turn * control.speed * dt, glm::two_pi<float>());
		});
	}

	void orbitSystem(nrd::Engine& engine, float dt)
	{
		engine.registry().each<Orbit, nrd::Transform>([&](Entity, Orbit& orbit, nrd::Transform& transform) {
			const glm::mat4 rotation = glm::rotate(glm::mat4{ 1.f }, orbit.speed * dt, orbit.axis);
			transform.position = orbit.centre + glm::vec3(rotation * glm::vec4(transform.position - orbit.centre, 1.f));
		});
	}

	void debugOverlayToggleSystem(nrd::Engine& engine, float dt)
	{
		if (engine.input().actionPressed("toggleDebug")) {
			engine.renderer().setDebugOverlay(!engine.renderer().debugOverlay());
		}
	}

	void attackSystem(nrd::Engine& engine, float dt)
	{
		nrd::ecs::Registry& registry = engine.registry();
		std::vector<Entity> defeated;
		registry.each<Combat, nrd::Transform>([&](Entity attacker, Combat& combat, nrd::Transform& transform) {
			const nrd::RigidBody* body = registry.tryGet<nrd::RigidBody>(attacker);
			const bool grounded = body && body->grounded;
			if (grounded) combat.airAttackUsed = false;

			const AttackMove* move = combat.current();
			if (!move) {
				combat.currentMove.clear();
				return;
			}

			combat.timer += dt;
			if (combat.timer >= move->duration || (move->airborne && grounded)) {
				combat.currentMove.clear();
				return;
			}
			if (combat.timer < move->activeStart || combat.timer > move->activeEnd) return;

			//the hitbox is authored facing right; sprites facing left mirror it
			const nrd::Sprite* sprite = registry.tryGet<nrd::Sprite>(attacker);
			const float facing = sprite && sprite->flipX ? -1.f : 1.f;
			const glm::vec3 offset{ move->hitboxOffset.x * facing, move->hitboxOffset.y, move->hitboxOffset.z };
			const nrd::Aabb hitbox = nrd::Aabb::fromCenterHalfExtents(transform.position + offset, move->hitboxHalfExtents);
			engine.renderer().drawDebugBox(hitbox);

			registry.each<Health, nrd::Collider, nrd::Transform>([&](Entity target, Health& health, nrd::Collider& collider, nrd::Transform& targetTransform) {
				if (target == attacker || health.hp <= 0) return;
				if (std::find(combat.hitThisAttack.begin(), combat.hitThisAttack.end(), target) != combat.hitThisAttack.end()) return;
				if (!hitbox.overlaps(collider.worldBounds(targetTransform.position))) return;

				combat.hitThisAttack.push_back(target);
				health.hp -= move->damage;
				if (nrd::RigidBody* body = registry.tryGet<nrd::RigidBody>(target)) body->velocity.x = move->knockback * facing;
				if (engine.debugLog) {
					std::cout << "hit " << combat.currentMove << " target " << target << " hp " << health.hp << "/" << health.maxHp << "\n";
				}
				if (health.hp <= 0) defeated.push_back(target);
			});
		});
		//destroyed after the loops: entities can't be removed while they're being iterated
		for (const Entity entity : defeated) registry.destroy(entity);
	}

	void characterAnimationSystem(nrd::Engine& engine, float dt)
	{
		nrd::ecs::Registry& registry = engine.registry();
		registry.each<CharacterAnimations, nrd::Sprite, nrd::RigidBody>(
			[&](Entity entity, CharacterAnimations& animations, nrd::Sprite& sprite, nrd::RigidBody& body) {
				//an attack overrides everything, and keeps the facing it started with
				if (Combat* combat = registry.tryGet<Combat>(entity); combat && combat->attacking()) {
					if (const AttackMove* move = combat->current()) sprite.animator.play(move->animation, combat->justStarted);
					combat->justStarted = false;
					return;
				}

				//face the way we're walking; the sheets are drawn facing right
				if (body.velocity.x < 0.f) sprite.flipX = true;
				else if (body.velocity.x > 0.f) sprite.flipX = false;

				const bool walking = body.velocity.x != 0.f || body.velocity.z != 0.f;
				if (!body.grounded) sprite.animator.play(animations.jump);
				else if (walking) sprite.animator.play(animations.walk);
				else sprite.animator.play(animations.idle);
			});
	}
}
