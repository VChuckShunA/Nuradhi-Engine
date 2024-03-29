#include "first_app.hpp"
#include "simple_render_system.hpp"
#include "point_light_system.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
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
	/*

	FirstApp::FirstApp()
	{
		globalPool = LveDescriptorPool::Builder(lveDevice)
			.setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
			.build();
		loadGameObjects();
	}
	FirstApp::~FirstApp()
	{
	}
	void lve::FirstApp::run()
	{
		std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < uboBuffers.size(); i++) {
			uboBuffers[i] = std::make_unique<LveBuffer>(
				lveDevice,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			uboBuffers[i]->map();
		}

		auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
			.build();

		std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < globalDescriptorSets.size(); i++) {
			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			LveDescriptorWriter(*globalSetLayout, *globalPool)
				.writeBuffer(0, &bufferInfo)
				.build(globalDescriptorSets[i]);
		}

		SimpleRenderSystem simpleRenderSystem{ 
			lveDevice,
			lveRenderer.getSwapChainRenderPass(),
			globalSetLayout->getDescriptorSetLayout()};

		PointLightSystem pointLightSystem{
			lveDevice,
			lveRenderer.getSwapChainRenderPass(),
			globalSetLayout->getDescriptorSetLayout() };

		LveCamera camera{};
		//camera.setViewDirection(glm::vec3(0.f), glm::vec3(0.5f, 0.f, 1.f)); //down the z axis, slightly to the right
		camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f)); //corresponds to the centre of the cube
		//camera.setViewTarget(glm::vec3(-1.f, -2.f, -20.f), glm::vec3(0.f, 0.f, 2.5f)); //Set the far plane to higher valeue not to be clipped at camera projection
		
		auto viewerObject = LveGameObject::createGameObject();
		viewerObject.transform.translation.z = -2.5f;//camera position
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
			camera.setPerspectiveProjection(glm::radians(50.f),aspect,0.1f,100.f);
			//camera.setOrthographicProjection(-aspect, aspect, -1, 1, 0.1f, 10.f);

			//begin frame function will return a null ptr if the swap chain needs to be created
			if (auto commandBuffer = lveRenderer.beginFrame()) {
				int frameIndex = lveRenderer.getFrameIndex();
				FrameInfo frameInfo{
					frameIndex,
					frameTime,
					commandBuffer,
					camera,
					globalDescriptorSets[frameIndex],
					gameObjects
				};
				//update
				GlobalUbo ubo{};
				ubo.projection = camera.getProjection();
				ubo.view = camera.getView();
				ubo.inverseView = camera.getInverseView();
				pointLightSystem.update(frameInfo, ubo);
				uboBuffers[frameIndex]->writeToBuffer(&ubo);
				uboBuffers[frameIndex]->flush();

				//render
				lveRenderer.beginSwapChainRenderPass(commandBuffer);
				
				//order here matters
				simpleRenderSystem.renderGameObects(frameInfo);
				pointLightSystem.render(frameInfo);
				
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
		flatVase.transform.translation = { -.5f,.5f,0.f };
		flatVase.transform.scale = {3.f,1.5f,3.f};
		gameObjects.emplace(flatVase.getId(),std::move(flatVase));

		lveModel = LveModel::createModelFromFile(lveDevice, "models/smooth_vase.obj");
		auto smoothVase = LveGameObject::createGameObject();
		smoothVase.model = lveModel;
		smoothVase.transform.translation = { .5f,.5f,0.f };
		smoothVase.transform.scale = { 3.f,1.5f,3.f };
		gameObjects.emplace(smoothVase.getId(), std::move(smoothVase));

		lveModel = LveModel::createModelFromFile(lveDevice, "models/quad.obj");
		auto floor = LveGameObject::createGameObject();
		floor.model = lveModel;
		floor.transform.translation = { 0.f,0.5f,0.f };
		floor.transform.scale = { 3.f,1.f,3.f };
		gameObjects.emplace(floor.getId(), std::move(floor));
		
		
		//pointlights
		
		//NOTE: using pointlight again is Invalid!

		std::vector<glm::vec3> lightColours{
	 {1.f, .1f, .1f},
	 {.1f, .1f, 1.f},
	 {.1f, 1.f, .1f},
	 {1.f, 1.f, .1f},
	 {.1f, 1.f, 1.f},
	 {1.f, 1.f, 1.f}  //
		};
	for (int i = 0; i < lightColours.size(); i++) {
			auto pointLight = LveGameObject::makePointLight(0.2f);
			pointLight.colour = lightColours[i];
			auto rotateLight = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>()) / lightColours.size(),
				{ 0.f,-1.f,0.f });
			pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
			gameObjects.emplace(pointLight.getId(), std::move(pointLight));

	}
	}

	
	*/
	
}