#pragma once

//libs
#include <glm/glm.hpp>

//std
#include <string>

namespace game {

	//Walks with the "moveX"/"moveZ" axes and jumps with the "jump" action. Needs a RigidBody.
	struct PlayerController {
		float moveSpeed = 0.5f; //world units per second
		float jumpSpeed = 3.5f; //upward speed at take-off; height is jumpSpeed^2 / (2 * gravity)
	};

	//Sprite animations a character plays, by name on its sprite sheet. Needs a Sprite and a RigidBody.
	struct CharacterAnimations {
		std::string idle;
		std::string walk;
		std::string jump; //anything airborne
	};

	//Circles the entity around a point
	struct Orbit {
		glm::vec3 centre{ 0.f };
		glm::vec3 axis{ 0.f, -1.f, 0.f };
		float speed = 1.f; //radians per second
	};

	//Turns the camera left and right with the "cameraYaw" axis
	struct CameraYawControl {
		float speed = 1.5f; //radians per second
	};
}
