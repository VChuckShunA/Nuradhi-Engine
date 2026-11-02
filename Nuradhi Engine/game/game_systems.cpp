#include "game/game_systems.hpp"
#include "game/characters/summers.hpp"
#include "game/game_components.hpp"
#include "engine/core/components.hpp"
#include "engine/effects/blood.hpp"
#include "engine/particles/particles.hpp"
#include "engine/physics/physics.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

//std
#include <algorithm>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

namespace game {

	using nrd::ecs::Entity;

	namespace {
		//can't act: reeling from a hit, or beaten (the player stays down at 0 hp, where the game over screen takes over)
		bool isStunned(nrd::ecs::Registry& registry, Entity entity)
		{
			const Health* health = registry.tryGet<Health>(entity);
			return health && (health->stunned() || health->defeated());
		}
	}

	void hitStunSystem(nrd::Engine& engine, float dt)
	{
		constexpr float FRICTION = 4.f; //world units per second squared: a knocked back fighter slides to a stop
		nrd::ecs::Registry& registry = engine.registry();
		registry.each<Health>([&](Entity entity, Health& health) {
			if (!health.stunned()) return;
			health.hitStun = std::max(0.f, health.hitStun - dt);
			if (nrd::RigidBody* body = registry.tryGet<nrd::RigidBody>(entity)) {
				glm::vec2 slide{ body->velocity.x, body->velocity.z };
				const float speed = glm::length(slide);
				if (speed > 0.f) slide *= std::max(0.f, speed - FRICTION * dt) / speed;
				body->velocity.x = slide.x;
				body->velocity.z = slide.y;
			}
		});
	}

	void playerAttackInputSystem(nrd::Engine& engine, float dt)
	{
		const nrd::Input& input = engine.input();
		nrd::ecs::Registry& registry = engine.registry();
		const AttackBindings defaults{};
		registry.each<PlayerController, Combat, nrd::RigidBody>([&](Entity entity, PlayerController&, Combat& combat, nrd::RigidBody& body) {
			//a move that waits while its button is held (see AttackMove::holdAt) needs to know whether it still is
			if (combat.attacking()) {
				combat.holding = !combat.heldAction.empty() && input.actionDown(combat.heldAction.c_str());
				return;
			}
			if (isStunned(registry, entity)) return;
			const AttackBindings* bound = registry.tryGet<AttackBindings>(entity);
			const AttackBindings& moves = bound ? *bound : defaults;

			//a button starts its move when pressed; a move that repeats starts again as long as the button is held
			const auto wants = [&](const char* action, const std::string& move) {
				if (move.empty() || !combat.moves.count(move)) return false;
				if (!combat.ready(move)) return false;
				return combat.moves.at(move).repeats ? input.actionDown(action) : input.actionPressed(action);
			};
			const auto begin = [&](const char* action, const std::string& move) {
				combat.start(move);
				combat.heldAction = action;
				combat.holding = true;
			};
			if (body.grounded) {
				if (wants("special", moves.special)) begin("special", moves.special);
				else if (wants("punch", moves.punch)) begin("punch", moves.punch);
				else if (wants("kick", moves.kick)) begin("kick", moves.kick);
			}
			else if (!combat.airAttackUsed) {
				const char* action = nullptr;
				const std::string* move = nullptr;
				if (wants("special", moves.jumpSpecial)) { action = "special"; move = &moves.jumpSpecial; }
				else if (wants("punch", moves.jumpPunch)) { action = "punch"; move = &moves.jumpPunch; }
				else if (wants("kick", moves.jumpKick)) { action = "kick"; move = &moves.jumpKick; }
				if (move) {
					begin(action, *move);
					combat.airAttackUsed = true;
				}
			}
		});
	}
	void weaponSystem(nrd::Engine& engine, float dt)
	{
		const nrd::Input& input = engine.input();
		nrd::ecs::Registry& registry = engine.registry();
		registry.each<Weapons, Combat, nrd::RigidBody, CharacterAnimations, AttackBindings>(
			[&](Entity entity, Weapons& weapons, Combat& combat, nrd::RigidBody& body, CharacterAnimations& animations, AttackBindings& bindings) {
				//not while reeling from a hit, mid-attack or in the air
				if (isStunned(registry, entity) || combat.attacking() || !body.grounded) return;

				WeaponMode wanted = weapons.mode;
				if (input.actionPressed("weapon1")) wanted = WeaponMode::Melee;
				else if (input.actionPressed("weapon2")) wanted = WeaponMode::Uzi;
				else if (input.actionPressed("weapon3")) wanted = WeaponMode::Katana;

				if (wanted != weapons.mode) {
					//a weapon coming out plays its own equip; going back to bare hands plays the one she's putting away, backwards
					const std::string swap = wanted == WeaponMode::Melee ? std::string("unequip.") + weaponName(weapons.mode) : std::string("equip.") + weaponName(wanted);
					setWeaponMode(animations, bindings, weapons, wanted);
					combat.start(swap);
					if (engine.debugLog) std::cout << "weapon " << weaponName(wanted) << "\n";
				}
				else if (input.actionPressed("pickup")) {
					combat.start(std::string(weaponName(weapons.mode)) + ".pickup");
				}
			});
	}
	void playerControlSystem(nrd::Engine& engine, float dt)
	{
		const nrd::Input& input = engine.input();
		nrd::ecs::Registry& registry = engine.registry();
		registry.each<PlayerController, nrd::RigidBody>([&](Entity entity, PlayerController& controller, nrd::RigidBody& body) {
			//beaten: she stands where she is
			if (const Health* health = registry.tryGet<Health>(entity); health && health->defeated()) {
				body.velocity.x = body.velocity.z = 0.f;
				controller.sprintDirection = 0.f;
				return;
			}

			//reeling from a hit: no control, the knockback slides out (hitStunSystem)
			if (isStunned(registry, entity)) {
				controller.sprintDirection = 0.f;
				return;
			}

			//committed to an attack: plant the feet, or keep flying on the jump's momentum
			const Combat* combat = registry.tryGet<Combat>(entity);
			if (combat && combat->attacking()) {
				const AttackMove* move = combat->current();
				if (!move || !move->airborne) {
					body.velocity.x = body.velocity.z = 0.f;
					controller.sprintDirection = 0.f; //a ground attack stops a sprint
				}
				return;
			}

			//sprint: double tap a direction on the ground, then keep holding it. Letting go or turning around stops.
			const float moveX = input.axis("moveX");
			const float moveZ = input.axis("moveZ");
			const float doubleTap = input.axisDoubleTapped("moveX");
			const float sprintBefore = controller.sprintDirection;
			if (doubleTap != 0.f && body.grounded) controller.sprintDirection = doubleTap;
			if (moveX != controller.sprintDirection) controller.sprintDirection = 0.f;
			if (engine.debugLog && controller.sprintDirection != sprintBefore) {
				std::cout << (controller.sprintDirection != 0.f ? "sprint start " : "sprint stop ") << controller.sprintDirection << "\n";
			}

			if (controller.sprintDirection != 0.f) {
				body.velocity.x = controller.sprintDirection * controller.sprintSpeed;
				body.velocity.z = moveZ * controller.moveSpeed;
			}
			else {
				//+z is forward, away from the camera
				glm::vec3 direction{ moveX, 0.f, moveZ };
				if (glm::dot(direction, direction) > 0.f) direction = glm::normalize(direction);
				body.velocity.x = direction.x * controller.moveSpeed;
				body.velocity.z = direction.z * controller.moveSpeed;
			}

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

	void filterToggleSystem(nrd::Engine& engine, float dt)
	{
		nrd::PostProcessSettings& filters = engine.renderer().postProcess();
		if (engine.input().actionPressed("toggleNoir")) filters.noir.enabled = !filters.noir.enabled;
		if (engine.input().actionPressed("toggleCrt")) filters.crt.enabled = !filters.crt.enabled;
	}

	namespace {
		//top of the highest static collider under a point (+y is down, so the smallest y at or below it); the point's
		//own height if there's nothing under it
		float floorBelow(nrd::ecs::Registry& registry, const glm::vec3& point)
		{
			float floor = std::numeric_limits<float>::max();
			registry.each<nrd::Collider, nrd::Transform>([&](Entity, nrd::Collider& collider, nrd::Transform& transform) {
				if (!collider.isStatic) return;
				const nrd::Aabb box = collider.worldBounds(transform.position);
				if (point.x < box.min.x || point.x > box.max.x || point.z < box.min.z || point.z > box.max.z) return;
				if (box.min.y >= point.y) floor = std::min(floor, box.min.y);
			});
			return floor == std::numeric_limits<float>::max() ? point.y : floor;
		}
	}

	namespace {
		//Hurts `target` where `contact` is: damage, hit stun, a shove along x in `direction` (+1 or -1), a spray of blood,
		//and whatever it was in the middle of swinging is interrupted. A target at 0 hp that is to be removed goes in `defeated`.
		void landHit(nrd::Engine& engine, Entity attacker, Entity target, Health& health, const glm::vec3& contact, float direction,
			int damage, float knockback, float hitStun, const std::string& what, std::vector<Entity>& defeated)
		{
			nrd::ecs::Registry& registry = engine.registry();
			//blood from where the blow meets the body, sprayed on along it and a little up
			nrd::spawnBlood(registry, contact, { direction, -0.6f, 0.f }, 0.6f + 0.4f * damage, floorBelow(registry, contact));
			health.hp = std::max(0, health.hp - damage);
			health.hitStun = std::max(health.hitStun, hitStun);
			if (nrd::RigidBody* body = registry.tryGet<nrd::RigidBody>(target)) {
				body->velocity.x = knockback * direction;
				body->velocity.z = 0.f;
			}
			//getting hit interrupts whatever the target was swinging
			if (Combat* targetCombat = registry.tryGet<Combat>(target)) targetCombat->currentMove.clear();
			if (engine.debugLog) {
				std::cout << "hit " << what << " attacker " << attacker << " target " << target << " hp " << health.hp << "/" << health.maxHp << "\n";
			}
			if (health.defeated() && health.destroyOnDefeat) defeated.push_back(target);
		}
	}

	void attackSystem(nrd::Engine& engine, float dt)
	{
		nrd::ecs::Registry& registry = engine.registry();
		std::vector<Entity> defeated;
		std::vector<std::pair<glm::vec3, Projectile>> fired; //bullets, made after the loop: entities can't be created while iterating
		registry.each<Combat, nrd::Transform>([&](Entity attacker, Combat& combat, nrd::Transform& transform) {
			const nrd::RigidBody* body = registry.tryGet<nrd::RigidBody>(attacker);
			const bool grounded = body && body->grounded;
			if (grounded) combat.airAttackUsed = false;
			for (auto& [name, remaining] : combat.cooldowns) remaining -= dt;

			const AttackMove* move = combat.current();
			if (!move) {
				combat.currentMove.clear();
				return;
			}

			combat.timer += dt;
			//a move that waits for its button (see AttackMove::holdAt): while it's held the clock stands still at that point
			if (move->holdAt > 0.f && combat.holding && combat.timer >= move->holdAt) combat.timer = move->holdAt;
			if (combat.timer >= move->duration || (move->airborne && grounded)) {
				if (move->cooldown > 0.f) combat.cooldowns[combat.cooldownKey(combat.currentMove)] = move->cooldown;
				if (engine.debugLog) std::cout << "attack end " << combat.currentMove << "\n";
				const std::string next = move->next;
				combat.currentMove.clear();
				if (!next.empty() && combat.moves.count(next)) combat.start(next); //e.g. lowering the guns after firing
				return;
			}
			if (combat.timer < move->activeStart || combat.timer > move->activeEnd) return;

			//the hitbox is authored facing right; sprites facing left mirror it
			const nrd::Sprite* sprite = registry.tryGet<nrd::Sprite>(attacker);
			const float facing = sprite && sprite->flipX ? -1.f : 1.f;

			//a shooting move has no hitbox: it fires a bullet every shotInterval, from the guns
			if (move->shotInterval > 0.f) {
				combat.shotTimer -= dt;
				if (combat.shotTimer <= 0.f) {
					combat.shotTimer += move->shotInterval;

					//the barrels of the guns as drawn in the frame on screen (points found on the sprite, mirrored when she
					//faces left); a sprite without any fires from muzzleOffset. Two guns take turns.
					std::vector<glm::vec3> muzzles;
					if (sprite) {
						if (const auto* points = sprite->animator.sheet().anchors(sprite->animator.currentFrameIndex(), "muzzle")) {
							const nrd::Collider* collider = registry.tryGet<nrd::Collider>(attacker);
							const std::optional<nrd::Aabb> bodyBox = collider ? std::optional<nrd::Aabb>{ collider->worldBounds(transform.position) } : std::nullopt;
							for (const glm::vec2& point : *points) muzzles.push_back(sprite->worldPoint(transform.position, bodyBox ? &*bodyBox : nullptr, point));
						}
					}
					if (muzzles.empty()) muzzles.push_back(transform.position + glm::vec3{ move->muzzleOffset.x * facing, move->muzzleOffset.y, move->muzzleOffset.z });
					const glm::vec3 muzzle = muzzles[static_cast<size_t>(combat.shotCount++) % muzzles.size()];
					Projectile bullet{};
					bullet.owner = attacker;
					bullet.velocity = { facing * move->bulletSpeed, 0.f, 0.f };
					bullet.damage = move->damage;
					bullet.knockback = move->knockback;
					bullet.hitStun = move->hitStun;
					bullet.lifeLeft = move->bulletLife;
					fired.push_back({ muzzle, bullet });
				}
				return;
			}

			const glm::vec3 offset{ move->hitboxOffset.x * facing, move->hitboxOffset.y, move->hitboxOffset.z };
			const nrd::Aabb hitbox = nrd::Aabb::fromCenterHalfExtents(transform.position + offset, move->hitboxHalfExtents);
			engine.renderer().drawDebugBox(hitbox);

			registry.each<Health, nrd::Collider, nrd::Transform>([&](Entity target, Health& health, nrd::Collider& collider, nrd::Transform& targetTransform) {
				if (target == attacker || health.hp <= 0) return;
				if (std::find(combat.hitThisAttack.begin(), combat.hitThisAttack.end(), target) != combat.hitThisAttack.end()) return;
				const nrd::Aabb targetBox = collider.worldBounds(targetTransform.position);
				if (!hitbox.overlaps(targetBox)) return;

				combat.hitThisAttack.push_back(target);
				landHit(engine, attacker, target, health, glm::clamp(hitbox.center(), targetBox.min, targetBox.max), facing,
					move->damage, move->knockback, move->hitStun, combat.currentMove, defeated);
			});
		});
		for (const auto& [position, bullet] : fired) {
			const Entity entity = registry.create();
			registry.add<nrd::Transform>(entity, nrd::Transform{ position });
			registry.add<Projectile>(entity, bullet);
		}
		//destroyed after the loops: entities can't be removed while they're being iterated
		for (const Entity entity : defeated) registry.destroy(entity);
	}

	void projectileSystem(nrd::Engine& engine, float dt)
	{
		nrd::ecs::Registry& registry = engine.registry();
		std::vector<Entity> spent, defeated;
		registry.each<Projectile, nrd::Transform>([&](Entity bullet, Projectile& shot, nrd::Transform& transform) {
			shot.lifeLeft -= dt;
			if (shot.lifeLeft <= 0.f) {
				spent.push_back(bullet);
				return;
			}

			//everything the bullet passes through this step, so a fast one can't skip over a thin target
			const glm::vec3 from = transform.position;
			const glm::vec3 to = from + shot.velocity * dt;
			const nrd::Aabb swept{ glm::min(from, to) - shot.halfExtents, glm::max(from, to) + shot.halfExtents };
			bool hit = false;
			registry.each<Health, nrd::Collider, nrd::Transform>([&](Entity target, Health& health, nrd::Collider& collider, nrd::Transform& targetTransform) {
				if (hit || target == shot.owner || health.hp <= 0) return;
				const nrd::Aabb targetBox = collider.worldBounds(targetTransform.position);
				if (!swept.overlaps(targetBox)) return;
				hit = true;
				landHit(engine, shot.owner, target, health, glm::clamp(to, targetBox.min, targetBox.max), shot.velocity.x < 0.f ? -1.f : 1.f,
					shot.damage, shot.knockback, shot.hitStun, "bullet", defeated);
			});
			if (hit) spent.push_back(bullet);
			else transform.position = to;
		});

		//the bullets are drawn as streaks by one particle emitter, whose particles are remade from the bullets every frame
		const Entity tracers = registry.findFirst<BulletTracers, nrd::ParticleEmitter>();
		if (tracers != nrd::ecs::NullEntity) {
			nrd::ParticleEmitter& emitter = registry.get<nrd::ParticleEmitter>(tracers);
			emitter.particles.clear();
			registry.each<Projectile, nrd::Transform>([&](Entity, Projectile& shot, nrd::Transform& transform) {
				nrd::Particle particle{};
				particle.position = transform.position;
				particle.velocity = shot.velocity;
				particle.size = 0.02f;
				particle.lifetime = 1.f;
				emitter.particles.push_back(particle);
			});
		}

		for (const Entity entity : spent) registry.destroy(entity);
		for (const Entity entity : defeated) registry.destroy(entity);
	}
	void characterAnimationSystem(nrd::Engine& engine, float dt)
	{
		nrd::ecs::Registry& registry = engine.registry();
		registry.each<CharacterAnimations, nrd::Sprite, nrd::RigidBody>(
			[&](Entity entity, CharacterAnimations& animations, nrd::Sprite& sprite, nrd::RigidBody& body) {
				//reeling from a hit overrides everything
				if (isStunned(registry, entity)) {
					if (!animations.hurt.empty()) sprite.animator.play(animations.hurt);
					return;
				}

				//an attack overrides the rest, and keeps the facing it started with
				if (Combat* combat = registry.tryGet<Combat>(entity); combat && combat->attacking()) {
					if (const AttackMove* move = combat->current()) sprite.animator.play(move->animation, combat->justStarted);
					combat->justStarted = false;
					return;
				}

				//face the way we're walking; the sheets are drawn facing right. (An AI fighter turns to face its target instead.)
				if (animations.faceMovement) {
					if (body.velocity.x < 0.f) sprite.flipX = true;
					else if (body.velocity.x > 0.f) sprite.flipX = false;
				}

				const float groundSpeed = glm::length(glm::vec2{ body.velocity.x, body.velocity.z });
				if (!body.grounded) sprite.animator.play(animations.jump);
				else if (!animations.run.empty() && groundSpeed >= animations.runSpeed) sprite.animator.play(animations.run);
				else if (groundSpeed > 0.f) sprite.animator.play(animations.walk);
				else sprite.animator.play(animations.idle);
			});
	}
}
