#include "nrd_movement_controller.hpp"

namespace nrd {
	NrdMovementController::NrdMovementController(NrdPlayer* player) :
		nrdPlayer(player),
		moveSpeed(player->moveSpeedMax)
	{
	}
	float NrdMovementController::floatDial(GLFWwindow* window, float value)
	{
		if (glfwGetKey(window, keys.vKey) == GLFW_PRESS) { return value += 0.5; }
		if (glfwGetKey(window, keys.vKey) == GLFW_PRESS) { return value -= 0.5; }
	}
	void NrdMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, lve::LveGameObject* player)
	{

		//no pitch clamp here: the sprite quad stands upright at rotation.x = pi/2, and clamping to 1.5 tilted it
		player->transform.rotation.y = glm::mod(player->transform.rotation.y, glm::two_pi<float>());


		float yaw = player->transform.rotation.y;
		const glm::vec3 forwardDir{ sin(yaw),0.f,cos(yaw) };
		const glm::vec3 rightDir{ forwardDir.z,0.f,-forwardDir.x };

		//vertical movement comes from gravity and jumping, not from keys
		glm::vec3 moveDir{ 0.f };
		if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
		if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
		if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
		if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
		if (glfwGetKey(window, keys.spaceKey) == GLFW_PRESS) nrdPlayer->jump(); //only takes effect while grounded


		//only normalize moveDir if it's NOT ZERO
		nrdPlayer->moveInput = glm::vec3{ 0.f };
		if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
			nrdPlayer->moveInput = glm::normalize(moveDir);
			player->transform.translation += moveSpeed * dt * nrdPlayer->moveInput;
		}
	}
	

}