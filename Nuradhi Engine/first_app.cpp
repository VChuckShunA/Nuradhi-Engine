#include "first_app.hpp"
#include "simple_render_system.hpp"
#include <stdexcept>
#include <array>
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
		while (!lveWindow.shouldClose()) {
			glfwPollEvents();

			//begin frame function will return a null ptr if the swap chain needs to be created
			if (auto commandBuffer = lveRenderer.beginFrame()) {
				//TODO:
				//begin off screen shadow pass
				//render shadow casting objects
				//end offscreen shadow pass
				lveRenderer.beginSwapChainRenderPass(commandBuffer);
				simpleRenderSystem.renderGameObect(commandBuffer,gameObjects);
				lveRenderer.endSwapChainRenderPass(commandBuffer);
				lveRenderer.endFrame();
			}
		}
		//The CPU will block until all GPU operations are completed
		vkDeviceWaitIdle(lveDevice.device());
	}
	void FirstApp::loadGameObjects()
	{
		std::vector<LveModel::Vertex> vertices{
			{{0.0f,-0.5f},{1.0f,0.0f,0.0f}},
			{{0.5f,0.5f},{0.0f,1.0f,0.0f}},
			{{-0.5f,0.5f},{0.0f,0.0f,1.0f}}
		};

		//using shared pointer to have one model to be used by multiple objects
		auto lveModel = std::make_shared<LveModel>(lveDevice,vertices);
		
		auto triangle = LveGameObject::createGameObject();
		triangle.model = lveModel;
		triangle.colour = { .1f,.8f,.1f };
		triangle.transform2d.translation.x = .2f;
		triangle.transform2d.scale = { 2.f,.5f };
		triangle.transform2d.rotation = .25f * glm::two_pi<float>();

		gameObjects.push_back(std::move(triangle));
	}
	
	
	
}