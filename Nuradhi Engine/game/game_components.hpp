#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

	//Walks with the "moveX"/"moveZ" axes and jumps with the "jump" action. Double tapping a "moveX" direction on
	//the ground and holding it sprints. Needs a RigidBody.
	struct PlayerController {
		float moveSpeed = 0.5f; //world units per second
		float sprintSpeed = 1.25f; //along x while sprinting; depth (z) stays at moveSpeed
		float jumpSpeed = 3.5f; //upward speed at take-off; height is jumpSpeed^2 / (2 * gravity)
		float sprintDirection = 0.f; //-1 or +1 while sprinting, 0 otherwise
	};

	//Sprite animations a character plays, by name on its sprite sheet. Needs a Sprite and a RigidBody.
	struct CharacterAnimations {
		std::string idle;
		std::string walk;
		std::string run; //at runSpeed or faster along the ground; leave empty to always walk
		std::string jump; //anything airborne
		std::string hurt; //while in hit stun (see Health); leave empty to keep the current pose
		float runSpeed = 0.8f;
	};

	//Circles the entity around a point
	struct Orbit {
		glm::vec3 centre{ 0.f };
		glm::vec3 axis{ 0.f, -1.f, 0.f };
		float speed = 1.f; //radians per second
	};

	//Turns the camera left and right with the "cameraYaw" axis
	struct CameraYawControl {
		float speed = 1.5f; //radians per second
	};

	//One attack. Times are seconds from the start of the attack; the hitbox is only live between activeStart and
	//activeEnd. Positions are world units from the attacker's position (its feet) as if facing right, and are
	//mirrored when it faces left. -y is up, so a hitbox at chest height has a negative y offset.
	struct AttackMove {
		std::string animation; //sprite animation to play, sized to last `duration`
		float duration = 0.3f; //the attacker can't walk, jump or attack again until it's over
		float activeStart = 0.1f;
		float activeEnd = 0.2f;
		glm::vec3 hitboxOffset{ 0.f }; //hitbox centre
		glm::vec3 hitboxHalfExtents{ 0.1f };
		int damage = 1;
		float knockback = 0.f; //horizontal speed given to a target that has a RigidBody
		float hitStun = 0.3f; //seconds the target is stunned: its attack is cancelled and it can't act
		//jump attack: only starts in the air, keeps the jump's momentum instead of planting the feet, and ends on
		//landing even if `duration` hasn't run out
		bool airborne = false;
		float cooldown = 0.f; //seconds after the move ends before it can be used again
		std::string cooldownGroup; //moves in the same group share one cooldown; empty means the move's own name
	};

	//Lets an entity attack. Game systems pick which move to start; attackSystem plays it out and lands hits on
	//entities with Health and a Collider.
	struct Combat {
		std::unordered_map<std::string, AttackMove> moves;
		std::string currentMove; //empty when not attacking
		float timer = 0.f;
		bool justStarted = false; //restart the move's animation even if the same move just ended
		bool airAttackUsed = false; //one jump attack per jump; cleared on landing
		std::vector<nrd::ecs::Entity> hitThisAttack; //each target is hit once per attack
		std::unordered_map<std::string, float> cooldowns; //seconds left per cooldown group; counted down by attackSystem

		bool attacking() const { return !currentMove.empty(); }
		std::string cooldownKey(const std::string& move) const {
			const auto found = moves.find(move);
			return found == moves.end() || found->second.cooldownGroup.empty() ? move : found->second.cooldownGroup;
		}
		bool ready(const std::string& move) const {
			const auto found = cooldowns.find(cooldownKey(move));
			return found == cooldowns.end() || found->second <= 0.f;
		}
		const AttackMove* current() const {
			const auto found = moves.find(currentMove);
			return found == moves.end() ? nullptr : &found->second;
		}
		void start(const std::string& move) {
			currentMove = move;
			timer = 0.f;
			justStarted = true;
			hitThisAttack.clear();
		}
	};

	//Can be hit. Each hit stuns for a moment; at 0 hp the entity is destroyed, unless destroyOnDefeat is off
	//(the player, for now: there's no game over yet).
	struct Health {
		int hp = 3;
		int maxHp = 3;
		float hitStun = 0.f; //seconds left; while > 0 the entity can't move or attack and slides to a stop
		bool destroyOnDefeat = true;

		bool stunned() const { return hitStun > 0.f; }
		bool defeated() const { return hp <= 0; }
	};

	//An AI fighter's settings, read by its behaviour tree (see enemy_ai.hpp)
	struct EnemyFighter {
		nrd::ecs::Entity target = nrd::ecs::NullEntity; //who to fight
		float moveSpeed = 0.4f;
		float aggroRange = 4.f; //notices the target within this distance
		float attackRange = 0.6f; //throws an attack when the target is this close along x...
		float depthTolerance = 0.15f; //...and lined up within this along z (depth)
		float recoverTime = 0.7f; //pause after each attack, so the target gets a chance to answer
		float kickChance = 0.35f; //otherwise it punches
	};
}
