#include "keyboard_movement_controller.hpp"
#include <iostream>
#include <glm/gtx/string_cast.hpp>
namespace lve {
	void KeyboardMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, LveGameObject& gameObject, lve::LveGameObject::Map& gameObjects)
	{
		glm::vec3 rotate{ 0 };
		if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
		if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
		if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
		if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

		//only normalize rotation if it's NOT ZERO
		 if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) 
		 {
			gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
		 }
		//prevent objects from going upside down
		gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f); 
		gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());


		float yaw = gameObject.transform.rotation.y;
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
			gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
		}

		LveGameObject& playerObject = *gameObjects[gameObject.index].get();
		// Use playerObject here as needed
		//printCordinates(gameObjects[gameObject.index]);
	}
	void KeyboardMovementController::printCordinates(std::shared_ptr<LveGameObject> gameObject)
	{
		glm::vec3 globalPosition = gameObject->transform.translation;
		std::cout << glm::to_string(globalPosition) << "\n";
	}
}