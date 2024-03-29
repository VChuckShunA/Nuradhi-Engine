#include "nrd_test_level.hpp"
#include "simple_render_system.hpp"
#include "point_light_system.hpp"
#include "lve_camera.hpp"
#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
#include "lve_texture.hpp"
#include "lve_pipeline.hpp"
#include "nrd_player.hpp"
#include "nrd_movement_controller.hpp"
#include "nrd_debugLinePipeline.hpp"
#include "nrd_debugLine.hpp"
#include "nrd_solidObject.hpp"
#include <stdexcept>
#include <array>
#include <chrono>
#include <cassert>
#include <glm/gtx/string_cast.hpp>
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
nrd::TestLevel::TestLevel()
{
	globalPool = lve::LveDescriptorPool::Builder(lveDevice)
		.setMaxSets(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT)
		.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT)
		.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT)
		.build();
	loadGameObjects();
}

nrd::TestLevel::~TestLevel()
{
}

void nrd::TestLevel::run()
{
	std::vector<std::unique_ptr<lve::LveBuffer>> uboBuffers(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < uboBuffers.size(); i++) {
		uboBuffers[i] = std::make_unique<lve::LveBuffer>(
			lveDevice,
			sizeof(lve::GlobalUbo),
			1,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		uboBuffers[i]->map();
	}

	auto globalSetLayout = lve::LveDescriptorSetLayout::Builder(lveDevice)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
		.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build();

	lve::Texture texture = lve::Texture(lveDevice, "Images/Sprites/Blaze.png");

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = texture.getSampler();
	imageInfo.imageView = texture.getImageView();
	imageInfo.imageLayout = texture.getImageLayout();

	std::vector<VkDescriptorSet> globalDescriptorSets(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < globalDescriptorSets.size(); i++) {
		auto bufferInfo = uboBuffers[i]->descriptorInfo();
		lve::LveDescriptorWriter(*globalSetLayout, *globalPool)
			.writeBuffer(0, &bufferInfo)
			.writeImage(1,&imageInfo)
			.build(globalDescriptorSets[i]);
	}

	lve::SimpleRenderSystem simpleRenderSystem{
		lveDevice,
		lveRenderer.getSwapChainRenderPass(),
		globalSetLayout->getDescriptorSetLayout() };

	lve::PointLightSystem pointLightSystem{
		lveDevice,
		lveRenderer.getSwapChainRenderPass(),
		globalSetLayout->getDescriptorSetLayout() };

	lve::LveCamera camera{};
	//camera.setViewDirection(glm::vec3(0.f), glm::vec3(0.5f, 0.f, 1.f)); //down the z axis, slightly to the right
	camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f)); //corresponds to the centre of the cube
	//camera.setViewTarget(glm::vec3(-1.f, -2.f, -20.f), glm::vec3(0.f, 0.f, 2.5f)); //Set the far plane to higher valeue not to be clipped at camera projection

	lve::LveGameObject viewerObject {};
	viewerObject.transform.translation.z = -13.5f;//camera position
	lve::KeyboardMovementController cameraController{};
	//creating the player Instance


	

	// Creating the player's game object
	std::shared_ptr<lve::LveModel> playerModel = lve::LveModel::createModelFromFile(lveDevice, "models/quad.obj");
	NrdPlayer* playerGO = new NrdPlayer(lveDevice);
	playerGO->index = gameObjects.size();
	playerGO->model = playerModel;
	playerGO->transform.translation = { 1.f,1.f,3.f };
	playerGO->transform.scale = { 0.5f,0.5f,0.5f };
	playerGO->transform.rotation = { 1.5708f,0.f,0.f };
	gameObjects.emplace_back(playerGO);
	/*
	//Flat vase GameObject
	std::shared_ptr<lve::LveModel> flatVaseModel = lve::LveModel::createModelFromFile(lveDevice, "models/flat_vase.obj");
	NrdSolidObject* flatVaseGo= new NrdSolidObject(lveDevice, -4, 4, -4, 0.4, 0, -0.1);
	flatVaseGo->index = gameObjects.size();
	flatVaseGo->model = flatVaseModel;
	flatVaseGo->transform.translation = {3.f,0.5f,0.5f };
	flatVaseGo->transform.scale ={ 1.0f,1.0f,1.0f };
	//flatVaseGo->transform.rotation = { 0.f,0.f,0.f };
	gameObjects.emplace_back(flatVaseGo);
	*/
	//Flat vase GameObject
	std::shared_ptr<lve::LveModel> flatVaseModel2 = lve::LveModel::createModelFromFile(lveDevice, "models/flat_vase.obj");
	NrdSolidObject* flatVaseGo2 = new NrdSolidObject(lveDevice, - .25, .25, -.25, .25, 0, -.5);
	flatVaseGo2->index = gameObjects.size();
	flatVaseGo2->model = flatVaseModel2;
	flatVaseGo2->transform.translation = { 2.f,1.5f,1.f };
	flatVaseGo2->transform.scale = { 1.0f,1.0f,1.0f };
	//flatVaseGo->transform.rotation = { 0.f,0.f,0.f };
	gameObjects.emplace_back(flatVaseGo2);

	//Floor GameObject
	std::shared_ptr<lve::LveModel> floorModel = lve::LveModel::createModelFromFile(lveDevice, "models/quad.obj");
	NrdSolidObject* floorGameObj = new NrdSolidObject(lveDevice, -1, 1, -1, 1, 0.1,0);
	//-.5, 1.5, -2, 2, .6, .5
	floorGameObj->index = gameObjects.size();
	floorGameObj->model = floorModel;
	floorGameObj->transform.translation = { 0.f,1.5f,2.f };
	floorGameObj->transform.scale = { 5.0f,1.0f,1.0f };
	//flatVaseGo->transform.rotation = { 0.f,0.f,0.f };
	gameObjects.emplace_back(floorGameObj);

	NrdSolidObject* floorGameObj2 = new NrdSolidObject(lveDevice, -1, 1, -1, 1, 0.1, 0);
	//-.5, 1.5, -2, 2, .6, .5
	floorGameObj2->index = gameObjects.size();
	floorGameObj2->model = floorModel;
	floorGameObj2->transform.translation = { 0.f,1.5f,4.f };
	floorGameObj2->transform.scale = { 5.0f,1.0f,1.0f };
	//flatVaseGo->transform.rotation = { 0.f,0.f,0.f };
	gameObjects.emplace_back(floorGameObj2);




	auto currentTime = std::chrono::high_resolution_clock::now();

	while (!lveWindow.shouldClose()) {
		glfwPollEvents();

		auto newTime = std::chrono::high_resolution_clock::now();
		float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
		currentTime = newTime;
		cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject,gameObjects);
		//camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

		
		//Player Controller
		nrd::NrdMovementController playerController(playerGO);
		playerController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, gameObjects[playerGO->index].get());

		camera.setViewYXZ({ gameObjects[playerGO->index].get()->transform.translation.x -0.4f,
							-0.75f ,
							-3.5f}, 
							{-0.3f,
			viewerObject.transform.rotation.y,
			viewerObject.transform.rotation.z
			});
		//std::cout << "distance to vase : "<<playerGO->distanceBetweenTwoEntities(playerGO, flatVaseGo) << "\n";
		
		//std::cout << std::to_string(NrdEntity::entities.size()) << "\n";
		
		float aspect = lveRenderer.getAspectRatio();
		
		//camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
		camera.setOrthographicProjection(-aspect, aspect, -1, 1, 0.1f, 100.f);

		//begin frame function will return a null ptr if the swap chain needs to be created
			auto commandBuffer = lveRenderer.beginFrame();
			if (!commandBuffer) continue;
			lveRenderer.beginSwapChainRenderPass(commandBuffer);
			int frameIndex = lveRenderer.getFrameIndex();
			lve::FrameInfo frameInfo{
				frameIndex,
				frameTime,
				commandBuffer,
				camera,
				globalDescriptorSets[frameIndex],
				gameObjects
			};


			simpleRenderSystem.getlvePipelinePtr()->bind(commandBuffer);
			lveRenderer.bindPipelineEssentials(commandBuffer, frameInfo);
			simpleRenderSystem.bindDescriptorSets(frameInfo);
			//update
			lve::GlobalUbo ubo{};
			ubo.projection = camera.getProjection();
			ubo.view = camera.getView();
			ubo.inverseView = camera.getInverseView();
			pointLightSystem.update(frameInfo, ubo);
			uboBuffers[frameIndex]->writeToBuffer(&ubo);
			uboBuffers[frameIndex]->flush();


			//order here matters
			simpleRenderSystem.renderGameObects(frameInfo);
			pointLightSystem.render(frameInfo);

			nrd::NrdDebugLinePipeline* debugPipelinePtr = simpleRenderSystem.getDebugPipelinePtr();
			debugPipelinePtr->bind(commandBuffer);
			lveRenderer.bindPipelineEssentials(commandBuffer, frameInfo);
			simpleRenderSystem.bindDescriptorSets(frameInfo);
			//render
			// Update and render the debug line
			simpleRenderSystem.renderGameObects(frameInfo);
			lveRenderer.endSwapChainRenderPass(commandBuffer);
			lveRenderer.endFrame();
		
	}
	//The CPU will block until all GPU operations are completed
	vkDeviceWaitIdle(lveDevice.device());
	playerGO->destroyDebugLine();
	playerGO->collider.destroyDebugLine();
	//flatVaseGo->destroyDebugLine();
	//flatVaseGo->collider.destroyDebugLine(); 
	flatVaseGo2->destroyDebugLine();
	flatVaseGo2->collider.destroyDebugLine(); 
	floorGameObj->destroyDebugLine();
	floorGameObj->collider.destroyDebugLine();
	floorGameObj2->destroyDebugLine();
	floorGameObj2->collider.destroyDebugLine();
			
}

void nrd::TestLevel::loadGameObjects()
{/*
	auto currentTime = std::chrono::high_resolution_clock::now();
	std::shared_ptr<lve::LveModel> lveModel = lve::LveModel::createModelFromFile(lveDevice, "models/flat_vase.obj");
	lve::LveGameObject* flatVase= new lve::LveGameObject{};
	flatVase->index = gameObjects.size();
	flatVase->model = lveModel;
	flatVase->transform.translation = { -.5f,.5f,0.f };
	flatVase->transform.scale = { 3.f,1.5f,3.f };
	gameObjects.emplace_back(flatVase);

	lveModel = lve::LveModel::createModelFromFile(lveDevice, "models/smooth_vase.obj");
	lve::LveGameObject* smoothVase = new lve::LveGameObject{};
	smoothVase->index = gameObjects.size();
	smoothVase->model = lveModel;
	smoothVase->transform.translation = { .5f,.5f,0.f };
	smoothVase->transform.scale = { 3.f,1.5f,3.f };
	gameObjects.emplace_back(smoothVase);

	lveModel = lve::LveModel::createModelFromFile(lveDevice, "models/quad.obj");
	lve::LveGameObject* floor= new lve::LveGameObject{};
	floor->index = gameObjects.size();
	floor->model = lveModel;
	floor->transform.translation = { 0.f,0.5f,0.f };
	floor->transform.scale = { 3.f,1.f,3.f };
	gameObjects.emplace_back(floor);

	*/
	


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
		auto pointLight = lve::LveGameObject::makePointLight(0.2f);
		pointLight->colour = lightColours[i];
		pointLight->index = gameObjects.size();
		auto rotateLight = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>()) / lightColours.size(),
			{ 0.f,-1.f,0.f });
		pointLight->transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
		gameObjects.emplace_back(std::move(pointLight));

	}
}
