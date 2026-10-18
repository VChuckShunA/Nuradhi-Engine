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
			input.bindAction("punch", Key::MouseLeft);
			input.bindAction("kick", Key::MouseRight);
			input.bindAction("special", Key::F);
		}

		//Blaze's attacks. Each animation's frames last exactly the move's duration (3 frames at 12 fps = 0.25 s),
		//and the hitbox is live on the middle frame, where the fist or foot is out.
		Combat blazeCombat()
		{
			Combat combat{};
			AttackMove punch{};
			punch.animation = "punch";
			punch.duration = 3.f / 12.f;
			punch.activeStart = 1.f / 12.f;
			punch.activeEnd = 2.f / 12.f;
			punch.hitboxOffset = { 0.38f, -0.72f, 0.f }; //a fist's length in front, at chest height
			punch.hitboxHalfExtents = { 0.14f, 0.1f, 0.25f };
			punch.damage = 1;
			punch.knockback = 0.5f;
			combat.moves["punch"] = punch;

			AttackMove kick{};
			kick.animation = "kick";
			kick.duration = 3.f / 10.f;
			kick.activeStart = 1.f / 10.f;
			kick.activeEnd = 2.f / 10.f;
			kick.hitboxOffset = { 0.55f, -0.55f, 0.f }; //longer reach, waist height
			kick.hitboxHalfExtents = { 0.25f, 0.12f, 0.25f };
			kick.damage = 2;
			kick.knockback = 1.f;
			combat.moves["kick"] = kick;

			//jump attacks hold their pose and stay live until she lands (a jump lasts about 0.7 s)
			AttackMove jumpPunch{}; //flying knee
			jumpPunch.animation = "jumpPunch";
			jumpPunch.duration = 0.6f;
			jumpPunch.activeStart = 0.05f;
			jumpPunch.activeEnd = 0.6f;
			jumpPunch.hitboxOffset = { 0.3f, -0.55f, 0.f }; //the knee, in front at mid height
			jumpPunch.hitboxHalfExtents = { 0.18f, 0.2f, 0.25f }; //tall enough to catch a standing head even at the top of the jump
			jumpPunch.damage = 1;
			jumpPunch.knockback = 0.8f;
			jumpPunch.airborne = true;
			combat.moves["jumpPunch"] = jumpPunch;

			AttackMove jumpKick{}; //diagonal flying kick
			jumpKick.animation = "jumpKick";
			jumpKick.duration = 0.6f;
			jumpKick.activeStart = 0.1f; //after the turn into the kick
			jumpKick.activeEnd = 0.6f;
			jumpKick.hitboxOffset = { 0.5f, -0.2f, 0.f }; //the foot, low and in front
			jumpKick.hitboxHalfExtents = { 0.25f, 0.15f, 0.25f };
			jumpKick.damage = 2;
			jumpKick.knockback = 1.2f;
			jumpKick.airborne = true;
			combat.moves["jumpKick"] = jumpKick;

			//special: an energy palm blast. Slow to come out, but the beam reaches well past a kick and hits
			//everything in it, so it can only be used every 1.5 s.
			AttackMove special{};
			special.animation = "special";
			special.duration = 6.f / 12.f;
			special.activeStart = 2.f / 12.f; //after the two charge frames
			special.activeEnd = 6.f / 12.f;
			special.hitboxOffset = { 0.8f, -0.72f, 0.f }; //the beam, from her palm forward at chest height
			special.hitboxHalfExtents = { 0.55f, 0.15f, 0.3f };
			special.damage = 3;
			special.knockback = 2.f;
			special.cooldown = 1.5f;
			special.cooldownGroup = "special";
			combat.moves["special"] = special;

			//jump special: a flaming dive. The fire wraps her body and trails out in front, so the hitbox is big;
			//it's her one air attack for the jump and shares the ground special's cooldown.
			AttackMove jumpSpecial{};
			jumpSpecial.animation = "jumpSpecial";
			jumpSpecial.duration = 0.7f;
			jumpSpecial.activeStart = 0.08f; //after the tuck
			jumpSpecial.activeEnd = 0.7f;
			jumpSpecial.hitboxOffset = { 0.45f, -0.45f, 0.f };
			jumpSpecial.hitboxHalfExtents = { 0.55f, 0.35f, 0.3f };
			jumpSpecial.damage = 3;
			jumpSpecial.knockback = 2.f;
			jumpSpecial.airborne = true;
			jumpSpecial.cooldown = 1.5f;
			jumpSpecial.cooldownGroup = "special";
			combat.moves["jumpSpecial"] = jumpSpecial;
			return combat;
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
			sheet->addRowAnimation("run", 8, 1, 6, 14.f); //row 8: six-frame sprint
			sheet->addRowAnimation("jump", row0, 10, 3, 8.f, false); //crouch, rise, then hold the falling pose
			const int row3 = 3, row4 = 4;
			sheet->addAnimation({ "punch", //wind up, straight jab, pull back
				{ sheet->frameAt(row3, 11), sheet->frameAt(row3, 10), sheet->frameAt(row3, 11) }, 12.f, false });
			sheet->addAnimation({ "kick", //chamber, front kick, recover
				{ sheet->frameAt(row4, 2), sheet->frameAt(row4, 0), sheet->frameAt(row4, 2) }, 10.f, false });
			const int row2 = 2; //energy blast
			sheet->addAnimation({ "special", //charge twice, thrust the palm, then hold the long beam
				{ sheet->frameAt(row2, 5), sheet->frameAt(row2, 6), sheet->frameAt(row2, 1), sheet->frameAt(row2, 2), sheet->frameAt(row2, 9), sheet->frameAt(row2, 9) },
				12.f, false });
			sheet->addAnimation({ "jumpSpecial", //tuck, then dive in a trail of fire until landing
				{ sheet->frameAt(7, 10), sheet->frameAt(7, 11) }, 12.f, false });
			const int row1 = 1; //airborne poses
			sheet->addAnimation({ "jumpPunch", { sheet->frameAt(row1, 1) }, 10.f, false }); //flying knee, held
			sheet->addAnimation({ "jumpKick", //turn into a diagonal kick, held until landing
				{ sheet->frameAt(row1, 0), sheet->frameAt(row1, 2) }, 10.f, false });
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
		const std::shared_ptr<SpriteSheet> blazeSheet = loadBlazeSheet(engine.debugLog);
		registry.add<Transform>(player, Transform{ { 1.f, 1.5f, 3.f } });
		registry.add<Sprite>(player, blazeSheet, blazeTexture, BLAZE_PIXELS_PER_UNIT).keepBackInsideCollider = true;
		registry.add<RigidBody>(player);
		registry.add<Collider>(player, Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }, false));
		registry.add<PlayerController>(player);
		registry.add<CharacterAnimations>(player, CharacterAnimations{ "idle", "walk", "run", "jump" });
		registry.add<Combat>(player, blazeCombat());

		//training dummy: a second Blaze, facing her from the left. She's solid and breaks after 5 points of damage.
		const Entity dummy = registry.create();
		registry.add<Transform>(dummy, Transform{ { -0.6f, 1.5f, 3.f } });
		Sprite& dummySprite = registry.add<Sprite>(dummy, blazeSheet, blazeTexture, BLAZE_PIXELS_PER_UNIT);
		dummySprite.animator.play("idle");
		dummySprite.keepBackInsideCollider = true;
		registry.add<Collider>(dummy, Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }));
		registry.add<Health>(dummy, Health{ 5, 5 });

		//Colliders are in world units around each entity's position, and +y is down: a box with offset y = -0.25
		//and half height 0.25 sits on top of its position, one with offset y = +0.05 hangs just below it.

		//vase standing on the floor
		const Entity vase = registry.create();
		registry.add<Transform>(vase, Transform{ { 2.f, 1.5f, 1.f } });
		registry.add<MeshRenderer>(vase, MeshRenderer{ vaseMesh, logoTexture });
		registry.add<Collider>(vase, Collider::box({ 0.25f, 0.25f, 0.25f }, { 0.f, -0.25f, 0.f }));
		registry.add<Health>(vase, Health{ 2, 2 }); //low: only kicks reach it

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

		engine.addSystem(Phase::Update, playerAttackInputSystem);
		engine.addSystem(Phase::Update, playerControlSystem);
		engine.addSystem(Phase::Update, cameraYawSystem);
		engine.addSystem(Phase::Update, orbitSystem);
		engine.addSystem(Phase::Update, debugOverlayToggleSystem);
		engine.addSystem(Phase::LateUpdate, attackSystem);
		engine.addSystem(Phase::LateUpdate, characterAnimationSystem);
	}
}
