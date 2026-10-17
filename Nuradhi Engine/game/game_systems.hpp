#pragma once

#include "engine/core/engine.hpp"

namespace game {

	//Phase::Update
	void playerControlSystem(nrd::Engine& engine, float dt); //input axes and actions -> RigidBody velocity, jumping
	void cameraYawSystem(nrd::Engine& engine, float dt);
	void orbitSystem(nrd::Engine& engine, float dt);
	void debugOverlayToggleSystem(nrd::Engine& engine, float dt); //"toggleDebug" action flips the debug overlay

	//Phase::LateUpdate, once physics has decided who's grounded
	void characterAnimationSystem(nrd::Engine& engine, float dt);
}
