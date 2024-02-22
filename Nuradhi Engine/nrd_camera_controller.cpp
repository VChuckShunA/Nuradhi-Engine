#include "nrd_camera_controller.hpp"

namespace nrd {
	void CameraController::update()
	{

		if (target != NULL) {
			//great, we're following someone
			float targetX = target->x;
			float targetY = target->y;

			//MOVE TO TARGET POSITION - has a camera drag
			//transform.trnaslation += ((targetX)-Globals::camera.x) * lerp * TimeController::timeController.dT;
			//camera.y += ((targetY)-Globals::camera.y) * lerp * TimeController::timeController.dT;
			
			//if you want the camera to be fixated on the hero
			//Globals::camera.x = targetX;
			//Globals::camera.y = targetY;
		}
		//else, don't follow
	}

}