#include "engine/core/engine.hpp"
#include "game/test_level.hpp"

//std
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

//  --debug-log   print collision contacts every frame and the sprite sheets' detected frames
int main(int argc, char** argv) {
	try {
		nrd::Engine engine{ { 1920, 1080, "Nuradhi - A Film Noir Game Engine" } };
		for (int i = 1; i < argc; i++) {
			if (std::string{ argv[i] } == "--debug-log") engine.debugLog = true;
		}

		game::loadTestLevel(engine);
		engine.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
