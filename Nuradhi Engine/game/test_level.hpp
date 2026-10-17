#pragma once

#include "engine/core/engine.hpp"

namespace game {

	//Blaze on two floor slabs next to a vase, lit by six orbiting point lights. Loads the assets, binds the
	//controls, spawns the entities and registers the game systems.
	//  WASD walk, Space jump, Left/Right turn the camera, F1 toggles the debug overlay
	void loadTestLevel(nrd::Engine& engine);
}
