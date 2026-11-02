#pragma once

#include "engine/core/engine.hpp"

namespace game {

	//Phase::Update
	void hitStunSystem(nrd::Engine& engine, float dt); //counts down hit stun and slides knocked back fighters to a stop; run first
	void weaponSystem(nrd::Engine& engine, float dt); //1/2/3 swap weapons and E picks up, for a character with Weapons; run before playerAttackInputSystem
	void playerAttackInputSystem(nrd::Engine& engine, float dt); //"punch"/"kick" actions start the moves in the character's AttackBindings; run before playerControlSystem
	void playerControlSystem(nrd::Engine& engine, float dt); //input axes and actions -> RigidBody velocity, jumping
	void cameraYawSystem(nrd::Engine& engine, float dt);
	void orbitSystem(nrd::Engine& engine, float dt);
	void debugOverlayToggleSystem(nrd::Engine& engine, float dt); //"toggleDebug" action flips the debug overlay
	void filterToggleSystem(nrd::Engine& engine, float dt); //"toggleNoir" and "toggleCrt" switch the full-screen filters

	//Phase::LateUpdate, once physics has moved everyone
	void attackSystem(nrd::Engine& engine, float dt); //times attacks, lands hits, removes entities whose Health runs out
	void projectileSystem(nrd::Engine& engine, float dt); //flies the bullets attackSystem fired and hurts what they reach; after attackSystem
	void characterAnimationSystem(nrd::Engine& engine, float dt); //after attackSystem
}
