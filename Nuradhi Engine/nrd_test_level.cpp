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
#include "nrd_solidObject.hpp"
#include "nrd_collision_debug.hpp"
#include "nrd_sprite.hpp"
#include <stdexcept>
#include <array>
#include <chrono>
#include <cassert>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
nrd::TestLevel::TestLevel()
{
	//every texture gets its own global descriptor set per frame, so the pool holds frames * textures sets
	globalPool = lve::LveDescriptorPool::Builder(lveDevice)
		.setMaxSets(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES)
		.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES)
		.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES)
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


	//textures are indexed by LveGameObject::textureIndex, index 0 is used by any object that doesn't pick one
	constexpr size_t defaultTextureIndex = 0;
	constexpr size_t playerTextureIndex = 1;
	std::vector<std::unique_ptr<lve::Texture>> textures;
	textures.push_back(std::make_unique<lve::Texture>(lveDevice, "Images/Sprites/Nuradhi Logo copy.png"));
	textures.push_back(std::make_unique<lve::Texture>(lveDevice, "Images/Sprites/Blaze.png", lve::TextureFilter::Pixelated));
	assert(textures.size() <= MAX_TEXTURES && "Raise TestLevel::MAX_TEXTURES, the descriptor pool is too small");

	//one global descriptor set per frame per texture: binding 0 is that frame's UBO, binding 1 is the texture
	//indexed as globalDescriptorSets[frameIndex][textureIndex]
	std::vector<std::vector<VkDescriptorSet>> globalDescriptorSets(
		lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT, std::vector<VkDescriptorSet>(textures.size()));
	for (int i = 0; i < globalDescriptorSets.size(); i++) {
		auto bufferInfo = uboBuffers[i]->descriptorInfo();
		for (size_t t = 0; t < textures.size(); t++) {
			VkDescriptorImageInfo imageInfo{};
			imageInfo.sampler = textures[t]->getSampler();
			imageInfo.imageView = textures[t]->getImageView();
			imageInfo.imageLayout = textures[t]->getImageLayout();

			bool built = lve::LveDescriptorWriter(*globalSetLayout, *globalPool)
				.writeBuffer(0, &bufferInfo)
				.writeImage(1, &imageInfo)
				.build(globalDescriptorSets[i][t]);
			if (!built) {
				throw std::runtime_error("Failed to allocate global descriptor set");
			}
		}
	}



	lve::SimpleRenderSystem simpleRenderSystem{
		lveDevice,
		lveRenderer.getSwapChainRenderPass(),
		globalSetLayout->getDescriptorSetLayout() };

	lve::PointLightSystem pointLightSystem{
		lveDevice,
		lveRenderer.getSwapChainRenderPass(),
		globalSetLayout->getDescriptorSetLayout() };

	nrd::ColliderDebugRenderer colliderDebugRenderer{ lveDevice };

	lve::LveCamera camera{};
	//camera.setViewDirection(glm::vec3(0.f), glm::vec3(0.5f, 0.f, 1.f)); //down the z axis, slightly to the right
	camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f)); //corresponds to the centre of the cube
	//camera.setViewTarget(glm::vec3(-1.f, -2.f, -20.f), glm::vec3(0.f, 0.f, 2.5f)); //Set the far plane to higher valeue not to be clipped at camera projection

	lve::LveGameObject viewerObject {};
	viewerObject.transform.translation.z = -13.5f;//camera position
	lve::KeyboardMovementController cameraController{};
	//creating the player Instance


	

	//Blaze's sprite sheet: the frames are found from the image itself, the animations pick which frames to play
	//(set NrdGlobals::debugging to print every detected frame with its row and column)
	auto blazeSheet = std::make_shared<SpriteSheet>(SpriteSheet::loadFromFile("Images/Sprites/Blaze.png"));
	const int blazeRow0 = 0; //stance, walk, punch, jump and kicks
	blazeSheet->addAnimation({ NrdPlayer::ANIM_IDLE, //fighting stance, breathing back and forth
		{ blazeSheet->frameAt(blazeRow0, 0), blazeSheet->frameAt(blazeRow0, 1), blazeSheet->frameAt(blazeRow0, 2), blazeSheet->frameAt(blazeRow0, 1) },
		6.f, true });
	blazeSheet->addRowAnimation(NrdPlayer::ANIM_WALK, blazeRow0, 3, 6, 10.f);
	blazeSheet->addRowAnimation(NrdPlayer::ANIM_JUMP, blazeRow0, 10, 3, 8.f, false); //crouch, rise, then hold the falling pose
	if (NrdGlobals::debugging) blazeSheet->describe(std::cout);

	// Creating the player's game object
	std::shared_ptr<lve::LveModel> spriteQuad = createSpriteQuad(lveDevice);
	NrdPlayer* playerGO = new NrdPlayer();
	playerGO->index = gameObjects.size();
	playerGO->textureIndex = playerTextureIndex;
	playerGO->model = spriteQuad;
	playerGO->sprite = std::make_unique<SpriteRenderer>(blazeSheet, BLAZE_PIXELS_PER_UNIT);
	playerGO->transform.translation = { 1.f,1.5f,3.f }; //feet on the floor
	gameObjects.emplace_back(playerGO);
	collisionWorld.add(*playerGO);

	//Colliders are in world units around each object's position, and +y is down: a box with offset y = -0.25
	//and half height 0.25 sits on top of its position, one with offset y = +0.05 hangs just below it.

	//Flat vase GameObject, standing on the floor
	std::shared_ptr<lve::LveModel> flatVaseModel2 = lve::LveModel::createModelFromFile(lveDevice, "models/flat_vase.obj");
	NrdSolidObject* flatVaseGo2 = new NrdSolidObject(Collider::box({ 0.25f, 0.25f, 0.25f }, { 0.f, -0.25f, 0.f }));
	flatVaseGo2->index = gameObjects.size();
	flatVaseGo2->model = flatVaseModel2;
	flatVaseGo2->transform.translation = { 2.f,1.5f,1.f };
	flatVaseGo2->transform.scale = { 1.0f,1.0f,1.0f };
	gameObjects.emplace_back(flatVaseGo2);
	collisionWorld.add(*flatVaseGo2);

	//Floor GameObjects: 10 x 2 slabs, 0.1 thick, whose top surface is at their position
	std::shared_ptr<lve::LveModel> floorModel = lve::LveModel::createModelFromFile(lveDevice, "models/quad.obj");
	const Collider floorCollider = Collider::box({ 5.f, 0.05f, 1.f }, { 0.f, 0.05f, 0.f });
	NrdSolidObject* floorGameObj = new NrdSolidObject(floorCollider);
	floorGameObj->index = gameObjects.size();
	floorGameObj->model = floorModel;
	floorGameObj->transform.translation = { 0.f,1.5f,2.f };
	floorGameObj->transform.scale = { 5.0f,1.0f,1.0f };
	gameObjects.emplace_back(floorGameObj);
	collisionWorld.add(*floorGameObj);

	NrdSolidObject* floorGameObj2 = new NrdSolidObject(floorCollider);
	floorGameObj2->index = gameObjects.size();
	floorGameObj2->model = floorModel;
	floorGameObj2->transform.translation = { 0.f,1.5f,4.f };
	floorGameObj2->transform.scale = { 5.0f,1.0f,1.0f };
	gameObjects.emplace_back(floorGameObj2);
	collisionWorld.add(*floorGameObj2);




	auto currentTime = std::chrono::high_resolution_clock::now();

	while (!lveWindow.shouldClose()) {
		glfwPollEvents();

		auto newTime = std::chrono::high_resolution_clock::now();
		float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
		currentTime = newTime;
		//the camera follows the player, so only its arrow-key yaw is used (WASD/QE move the player)
		cameraController.lookAround(lveWindow.getGLFWwindow(), frameTime, viewerObject);
		//camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

		
		//Player Controller
		nrd::NrdMovementController playerController(playerGO);
		playerController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, gameObjects[playerGO->index].get());
		//physics runs once per frame after input, before anything is drawn
		playerGO->applyGravity(frameTime);
		playerGO->resolveCollisions(collisionWorld);
		playerGO->updateAnimation(frameTime);

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
				globalDescriptorSets[frameIndex][defaultTextureIndex],
				gameObjects,
				globalDescriptorSets[frameIndex]
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
			colliderDebugRenderer.render(frameInfo, simpleRenderSystem.getPipelineLayout(), collisionWorld);
			lveRenderer.endSwapChainRenderPass(commandBuffer);
			lveRenderer.endFrame();
		
	}
	//The CPU will block until all GPU operations are completed
	vkDeviceWaitIdle(lveDevice.device());
}

void nrd::TestLevel::loadGameObjects()
{


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
