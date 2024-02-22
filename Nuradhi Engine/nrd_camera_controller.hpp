#pragma once

#include "lve_camera.hpp"
#include "nrd_entity.hpp"

namespace nrd {
	class CameraController: public lve::LveCamera {
	public:
		nrd::NrdEntity* target = NULL;
		float lerp = 2.6f;

		void update();
	};
}