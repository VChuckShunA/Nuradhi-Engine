#include "game/test_level.hpp"
#include "game/game_components.hpp"
#include "game/game_systems.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/collision.hpp"
#include "engine/physics/physics.hpp"
#include "engine/render/render_components.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

//std
#include <iostream>
#include <memory>
#include <vector>

namespace game {

	using namespace nrd;
	using ecs::Entity;

	namespace {
		constexpr float BLAZE_PIXELS_PER_UNIT = 75.f; //sheet pixels per world unit: her ~75 px idle pose stands 1 unit tall, like her collider

		void bindControls(Input& input)
		{
			input.bindAxis("moveX", Key::A, Key::D);
			input.bindAxis("moveZ", Key::S, Key::W);
			input.bindAction("jump", Key::Space);
			input.bindAxis("cameraYaw", Key::Left, Key::Right);
			input.bindAction("toggleDebug", Key::F1);
		}

		//Blaze's sprite sheet: the frames are found from the image itself, the animations pick which frames to play
		std::shared_ptr<SpriteSheet> loadBlazeSheet(bool describe)
		{
			auto sheet = std::make_shared<SpriteSheet>(SpriteSheet::loadFromFile("Images/Sprites/Blaze.png"));
			const int row0 = 0; //stance, walk, punch, jump and kicks
			sheet->addAnimation({ "idle", //fighting stance, breathing back and forth
				{ sheet->frameAt(row0, 0), sheet->frameAt(row0, 1), sheet->frameAt(row0, 2), sheet->frameAt(row0, 1) },
				6.f, true });
			sheet->addRowAnimation("walk", row0, 3, 6, 10.f);
			sheet->addRowAnimation("jump", row0, 10, 3, 8.f, false); //crouch, rise, then hold the falling pose
			if (describe) sheet->describe(std::cout); //every detected frame with its row and column
			return sheet;
		}

		void spawnLights(ecs::Registry& registry)
		{
			const std::vector<glm::vec3> colours{
				{1.f, .1f, .1f},
				{.1f, .1f, 1.f},
				{.1f, 1.f, .1f},
				{1.f, 1.f, .1f},
				{.1f, 1.f, 1.f},
				{1.f, 1.f, 1.f},
			};
			for (size_t i = 0; i < colours.size(); i++) {
				//evenly spaced on a circle around the origin, then orbiting it
				const glm::mat4 placement = glm::rotate(glm::mat4{ 1.f }, (i * glm::two_pi<float>()) / colours.size(), { 0.f, -1.f, 0.f });
				const Entity light = registry.create();
				registry.add<Transform>(light, Transform{ glm::vec3(placement * glm::vec4(-1.f, -1.f, -1.f, 1.f)) });
				registry.add<PointLight>(light, PointLight{ colours[i], 0.2f, 0.1f });
				registry.add<Orbit>(light, Orbit{ glm::vec3{ 0.f }, glm::vec3{ 0.f, -1.f, 0.f }, 1.f });
			}
		}
	}

	void loadTestLevel(Engine& engine)
	{
		ecs::Registry& registry = engine.registry();
		Renderer& renderer = engine.renderer();
		bindControls(engine.input());

		const TextureHandle logoTexture = renderer.loadTexture("Images/Sprites/Nuradhi Logo copy.png");
		const TextureHandle blazeTexture = renderer.loadTexture("Images/Sprites/Blaze.png", TextureFilter::Pixelated);
		const MeshHandle vaseMesh = renderer.loadMesh("models/flat_vase.obj");
		const MeshHandle quadMesh = renderer.loadMesh("models/quad.obj");

		spawnLights(registry);

		//Blaze. Her position is her feet, like the sprite's pivot, so her body box sits on top of it (-y is up):
		//1 unit tall like the sprite, half a unit wide and deep.
		const Entity player = registry.create();
		registry.add<Transform>(player, Transform{ { 1.f, 1.5f, 3.f } });
		registry.add<Sprite>(player, loadBlazeSheet(engine.debugLog), blazeTexture, BLAZE_PIXELS_PER_UNIT);
		registry.add<RigidBody>(player);
		registry.add<Collider>(player, Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }, false));
		registry.add<PlayerController>(player);
		registry.add<CharacterAnimations>(player, CharacterAnimations{ "idle", "walk", "jump" });

		//Colliders are in world units around each entity's position, and +y is down: a box with offset y = -0.25
		//and half height 0.25 sits on top of its position, one with offset y = +0.05 hangs just below it.

		//vase standing on the floor
		const Entity vase = registry.create();
		registry.add<Transform>(vase, Transform{ { 2.f, 1.5f, 1.f } });
		registry.add<MeshRenderer>(vase, MeshRenderer{ vaseMesh, logoTexture });
		registry.add<Collider>(vase, Collider::box({ 0.25f, 0.25f, 0.25f }, { 0.f, -0.25f, 0.f }));

		//two 10 x 2 floor slabs, 0.1 thick, whose top surface is at their position
		for (const float z : { 2.f, 4.f }) {
			const Entity floor = registry.create();
			registry.add<Transform>(floor, Transform{ { 0.f, 1.5f, z }, glm::vec3{ 0.f }, { 5.f, 1.f, 1.f } });
			registry.add<MeshRenderer>(floor, MeshRenderer{ quadMesh, logoTexture });
			registry.add<Collider>(floor, Collider::box({ 5.f, 0.05f, 1.f }, { 0.f, 0.05f, 0.f }));
		}

		//orthographic side view, following Blaze along x, tilted down a little
		const Entity camera = registry.create();
		registry.add<Transform>(camera, Transform{ { 1.f - 0.4f, -0.75f, -3.5f }, { -0.3f, 0.f, 0.f } });
		registry.add<Camera>(camera);
		registry.add<CameraFollow>(camera, CameraFollow{ player, { true, false, false }, { -0.4f, 0.f, 0.f } });
		registry.add<CameraYawControl>(camera);

		engine.addSystem(Phase::Update, playerControlSystem);
		engine.addSystem(Phase::Update, cameraYawSystem);
		engine.addSystem(Phase::Update, orbitSystem);
		engine.addSystem(Phase::Update, debugOverlayToggleSystem);
		engine.addSystem(Phase::LateUpdate, characterAnimationSystem);
	}
}
