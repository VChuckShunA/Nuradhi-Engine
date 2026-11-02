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
		bool faceMovement = true; //turn to face the way it walks; off for AI fighters, whose tree turns them to face their target
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
		//Shooting: with shotInterval > 0 the move has no hitbox of its own. Between activeStart and activeEnd it fires a
		//bullet every shotInterval seconds, which flies on at bulletSpeed and does the move's damage, knockback and hit
		//stun to the first thing it reaches. A bullet leaves from the barrel of a gun as drawn in the sprite's current frame
		//(the frame's `muzzle` anchor points, see SpriteSheet::setAnchors; with several, the guns take turns), or from
		//muzzleOffset (like hitboxOffset) if the frame has none.
		float shotInterval = 0.f;
		glm::vec3 muzzleOffset{ 0.f };
		float bulletSpeed = 10.f;
		float bulletLife = 0.5f; //seconds before a bullet that hit nothing disappears
		bool repeats = false; //on the ground, holding the attack button starts the move again instead of needing a new press
		//Holding: when holdAt > 0 and the button that started the move is still down, the move waits at that time instead of
		//running on, so the animation holds its last frame and a shooting move keeps firing, for as long as it's held.
		//Letting go carries on to the end of the move. A shooting move should end its firing window (activeEnd) at holdAt,
		//so nothing fires after the release.
		float holdAt = 0.f;
		std::string next; //a move to start as soon as this one runs its course (not when it's interrupted), e.g. lowering the guns
	};

	//Lets an entity attack. Game systems pick which move to start; attackSystem plays it out and lands hits on
	//entities with Health and a Collider.
	struct Combat {
		std::unordered_map<std::string, AttackMove> moves;
		std::string currentMove; //empty when not attacking
		float timer = 0.f;
		bool justStarted = false; //restart the move's animation even if the same move just ended
		float shotTimer = 0.f; //counts down to the next bullet of a shooting move
		int shotCount = 0; //bullets fired by this move so far: a pair of guns takes turns
		std::string heldAction; //the input action that started the move; the player's input system keeps `holding` up to date from it
		bool holding = false; //that action is still down
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
			shotTimer = 0.f;
			shotCount = 0;
			holding = false;
			heldAction.clear();
		}
	};

	//Can be hit. Each hit stuns for a moment; at 0 hp the entity is destroyed, unless destroyOnDefeat is off
	//(the player: she stays down at 0 hp, and the game over screen takes over).
	struct Health {
		int hp = 3;
		int maxHp = 3;
		float hitStun = 0.f; //seconds left; while > 0 the entity can't move or attack and slides to a stop
		bool destroyOnDefeat = true;

		bool stunned() const { return hitStun > 0.f; }
		bool defeated() const { return hp <= 0; }
	};

	//A bullet in flight. Its Transform is where it is; projectileSystem moves it, hurts whatever it reaches (anything
	//with Health and a Collider except its owner) and removes it.
	struct Projectile {
		nrd::ecs::Entity owner = nrd::ecs::NullEntity;
		glm::vec3 velocity{ 0.f };
		glm::vec3 halfExtents{ 0.06f, 0.06f, 0.12f };
		int damage = 1;
		float knockback = 0.f;
		float hitStun = 0.15f;
		float lifeLeft = 0.5f;
	};

	//Marks the entity whose ParticleEmitter draws every bullet in flight as a streak (see projectileSystem)
	struct BulletTracers {};

	//What a player character's attack actions start: the name of the Combat move for each, empty for none. Without this
	//component the defaults are used, which are the names Blaze's moves have.
	struct AttackBindings {
		std::string punch = "punch"; //left mouse button, on the ground
		std::string kick = "kick"; //right mouse button, on the ground
		std::string jumpPunch = "jumpPunch"; //left mouse button, in the air
		std::string jumpKick = "jumpKick"; //right mouse button, in the air
		std::string special = "special";
		std::string jumpSpecial = "jumpSpecial";
	};

	//Summers' three ways to fight, switched with the 1, 2 and 3 keys
	enum class WeaponMode { Melee, Uzi, Katana };

	struct Weapons {
		WeaponMode mode = WeaponMode::Melee;
	};

	//An AI fighter, run by the behaviour tree in enemy_ai.cpp. Without an EnemySquad it fights alone, always closing in
	//and attacking; with one, the squad system hands out attack turns so only a few fight at once while the rest wait
	//around the target (see EnemySquad). Settings are per enemy, so a crowd isn't a row of clones.
	struct EnemyFighter {
		nrd::ecs::Entity target = nrd::ecs::NullEntity; //who to fight
		float moveSpeed = 0.4f; //world units per second
		float aggroRange = 6.f; //notices the target within this distance
		float punchReach = 0.65f; //the furthest along x it still throws a punch from, once it's winding up
		float kickReach = 0.95f; //...and a kick
		float depthTolerance = 0.15f; //it attacks once lined up with the target within this along z (depth)
		float engageDistance = 0.6f; //how close along x it walks up to before attacking; set by planNextAttack for each attack
		bool plannedKick = false; //whether the next attack is a kick (thrown from further away) rather than a punch
		float windup = 0.25f; //seconds it stops and faces the target before swinging: the player's warning
		float recoverTime = 0.7f; //pause after each attack, so the target gets a chance to answer
		float kickChance = 0.35f; //how often the next attack is planned as a kick

		//--- run by the squad system; the tree reads them ---
		enum class Role { Attacker, Waiter };
		Role role = Role::Attacker; //a waiter hangs back at its slot until the squad sends it in
		glm::vec2 slot{ 0.f }; //x and z of where a waiter stands, around the target
		float cooldown = 0.f; //seconds before a waiter can be sent in again
		float waited = 0.f; //seconds spent waiting: the longest wait goes in first
		float turnTime = 0.f; //seconds as an attacker, so one that can't reach its target gives up its turn
		bool swinging = false; //the squad system's note of whether it was mid-attack last frame, for its log
		bool attackDone = false; //the tree sets this when an attack and its recovery are over; the squad takes its turn back
	};

	//The rules for a crowd of EnemyFighters, on an entity of its own. Every frame the squad decides which enemies attack
	//and which wait: at most maxAttackers at once, sent in one at a time with attackGap between them, preferring to
	//send them in from opposite sides of the target so it is flanked. An enemy that finishes an attack, or is hit,
	//goes back to waiting for a moment, so the same one doesn't attack over and over while the others stand idle.
	struct EnemySquad {
		int maxAttackers = 2;
		float attackGap = 0.7f; //seconds between one enemy being sent in and the next
		float standoff = 1.6f; //how far along x from the target the first waiter on each side stands: clear of where the attackers walk in
		float slotSpacing = 0.45f; //further out for each extra waiter on the same side
		float minCooldown = 0.9f; //an enemy that has had its turn waits at least this long...
		float maxCooldown = 1.9f; //...and up to this long, before it can have another
		glm::vec2 depthRange{ 1.5f, 4.5f }; //where along z waiters may stand: inside the floor
		float time = 0.f;
		float gapTimer = 0.f; //counts down to when the next enemy may be sent in
	};
}