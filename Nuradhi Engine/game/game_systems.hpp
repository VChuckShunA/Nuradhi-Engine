#pragma once

#include "engine/core/engine.hpp"

namespace game {

	//Phase::Update
	void hitStunSystem(nrd::Engine& engine, float dt); //counts down hit stun and slides knocked back fighters to a stop; run first
	void playerAttackInputSystem(nrd::Engine& engine, float dt); //"punch"/"kick" actions start Combat moves; run before playerControlSystem
	void playerControlSystem(nrd::Engine& engine, float dt); //input axes and actions -> RigidBody velocity, jumping
	void cameraYawSystem(nrd::Engine& engine, float dt);
	void orbitSystem(nrd::Engine& engine, float dt);
	void debugOverlayToggleSystem(nrd::Engine& engine, float dt); //"toggleDebug" action flips the debug overlay

	//Phase::LateUpdate, once physics has moved everyone
	void attackSystem(nrd::Engine& engine, float dt); //times attacks, lands hits, removes entities whose Health runs out
	void characterAnimationSystem(nrd::Engine& engine, float dt); //after attackSystem
}
