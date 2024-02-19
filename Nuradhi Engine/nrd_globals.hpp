#pragma once

#include <string>
#include <iostream>
#include "nrd_random_number.hpp"

namespace nrd {
	class NrdGlobals {
	public:
		//math helpers
		static const float PI;

		//useful for me as a dev
		static bool debugging;

		//sdl related 
		static int ScreenWidth, ScreenHeight, ScreenScale;
		//static SDL_Renderer* renderer;

		//clips off header e.g "clip: 50 114 44 49" turns into "50 114 44 49" 
		static std::string clipOffDataHeader(std::string data);

		//camera
		//static SDL_Rect camera;
	};
}
