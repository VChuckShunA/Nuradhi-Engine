#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

	//Walks with the "moveX"/"moveZ" axes and jumps with the "jump" action. Needs a RigidBody.
	struct PlayerController {
		float moveSpeed = 0.5f; //world units per second
		float jumpSpeed = 3.5f; //upward speed at take-off; height is jumpSpeed^2 / (2 * gravity)
	};

	//Sprite animations a character plays, by name on its sprite sheet. Needs a Sprite and a RigidBody.
	struct CharacterAnimations {
		std::string idle;
		std::string walk;
		std::string jump; //anything airborne
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
		//jump attack: only starts in the air, keeps the jump's momentum instead of planting the feet, and ends on
		//landing even if `duration` hasn't run out
		bool airborne = false;
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

		bool attacking() const { return !currentMove.empty(); }
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

	//Can be hit; the entity is destroyed when hp reaches 0
	struct Health {
		int hp = 3;
		int maxHp = 3;
	};
}
