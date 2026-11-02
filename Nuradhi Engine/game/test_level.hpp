#pragma once

#include "engine/core/engine.hpp"

namespace game {

	//Blaze on two floor slabs next to a vase, lit by six orbiting point lights, with an AI-driven second Blaze to
	//spar against, in the rain and a low fog (weather can be switched in the level editor). Loads the assets, binds the
	//controls, spawns the entities and registers the game systems.
	//  WASD walk (double tap and hold A or D to sprint), Space jump, left click punch, right click kick (in the air: flying knee and flying kick),
	//  F special (energy blast; in the air a flaming dive; shared 1.5 s cooldown),
	//  Left/Right turn the camera, F1 toggles the debug lines (off by default; there's a checkbox in the editor too), F2 opens the level editor, F3 the film noir filter, F4 the CRT filter,
	//  P pauses and resumes, and R starts over from the game over screen
	void loadTestLevel(nrd::Engine& engine);
}
