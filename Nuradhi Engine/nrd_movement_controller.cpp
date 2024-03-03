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
		
		//prevent objects from going upside down
		player->transform.rotation.x = glm::clamp(player->transform.rotation.x, -1.5f, 1.5f);
		player->transform.rotation.y = glm::mod(player->transform.rotation.y, glm::two_pi<float>());


		float yaw = player->transform.rotation.y;
		const glm::vec3 forwardDir{ sin(yaw),0.f,cos(yaw) };
		const glm::vec3 rightDir{ forwardDir.z,0.f,-forwardDir.x };
		const glm::vec3 upDir{ 0.f,-1.f,0.f };

		glm::vec3 moveDir{ 0.f };
		if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
		if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
		if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
		if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
		if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
		if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;


		//only normalize moveDir if it's NOT ZERO
		if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
			player->transform.translation += moveSpeed * dt * glm::normalize(moveDir);
		}
	}
	

}