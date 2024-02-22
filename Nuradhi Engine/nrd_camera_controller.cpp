#include "nrd_camera_controller.hpp"

namespace nrd {
	void CameraController::update()
	{

		if (target != NULL) {
			//great, we're following someone
			//float targetX = target->x - lve::LveCamera.WIDTH / 2;
			//float targetY = target->y - lve::LveCamera.HEIGHT / 2;

			//MOVE TO TARGET POSITION - has a camera drag
			//lve::LveCamera.transform += ((targetX)-Globals::camera.x) * lerp * TimeController::timeController.dT;
			//Globals::camera.y += ((targetY)-Globals::camera.y) * lerp * TimeController::timeController.dT;
			
			//if you want the camera to be fixated on the hero
			//Globals::camera.x = targetX;
			//Globals::camera.y = targetY;
		}
		//else, don't follow
	}

}