#include "first_app.hpp"
#include "simple_render_system.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"
#include <stdexcept>
#include <array>
#include <chrono>
#include <cassert>
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>


namespace lve {
	
	FirstApp::FirstApp()
	{
		loadGameObjects();
	}
	FirstApp::~FirstApp()
	{
	}
	void lve::FirstApp::run()
	{
		SimpleRenderSystem simpleRenderSystem{ lveDevice,lveRenderer.getSwapChainRenderPass()};
		LveCamera camera{};
		//camera.setViewDirection(glm::vec3(0.f), glm::vec3(0.5f, 0.f, 1.f)); //down the z axis, slightly to the right
		camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f)); //corresponds to the centre of the cube
		//camera.setViewTarget(glm::vec3(-1.f, -2.f, -20.f), glm::vec3(0.f, 0.f, 2.5f)); //Set the far plane to higher valeue not to be clipped at camera projection
		
		auto viewerObject = LveGameObject::createGameObject();
		KeyboardMovementController cameraController{};

		auto currentTime = std::chrono::high_resolution_clock::now();
		
		while (!lveWindow.shouldClose()) {
			glfwPollEvents();

			auto newTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
			camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

			float aspect = lveRenderer.getAspectRatio();
			//camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
			camera.setPerspectiveProjection(glm::radians(50.f),aspect,0.1f,10.f);
			//camera.setOrthographicProjection(-aspect, aspect, -1, 1, 0.1f, 10.f);

			//begin frame function will return a null ptr if the swap chain needs to be created
			if (auto commandBuffer = lveRenderer.beginFrame()) {
				//TODO:
				//begin off screen shadow pass
				//render shadow casting objects
				//end offscreen shadow pass
				lveRenderer.beginSwapChainRenderPass(commandBuffer);
				simpleRenderSystem.renderGameObect(commandBuffer,gameObjects, camera);
				lveRenderer.endSwapChainRenderPass(commandBuffer);
				lveRenderer.endFrame();
			}
		}
		//The CPU will block until all GPU operations are completed
		vkDeviceWaitIdle(lveDevice.device());
	}

	

	void FirstApp::loadGameObjects()
	{
		std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice,"models/flat_vase.obj");
		auto flatVase = LveGameObject::createGameObject();
		flatVase.model = lveModel;
		flatVase.transform.translation = { -.5f,.5f,2.5f };
		flatVase.transform.scale = {3.f,1.5f,3.f};
		gameObjects.push_back(std::move(flatVase));

		lveModel = LveModel::createModelFromFile(lveDevice, "models/smooth_vase.obj");
		auto smoothVase = LveGameObject::createGameObject();
		smoothVase.model = lveModel;
		smoothVase.transform.translation = { .5f,.5f,2.5f };
		smoothVase.transform.scale = { 3.f,1.5f,3.f };
		gameObjects.push_back(std::move(smoothVase));
	}
	
}