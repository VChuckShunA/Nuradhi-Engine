#include "game/test_level.hpp"
#include "game/archive/blaze.hpp"
#include "game/characters/summers.hpp"
#include "game/game_components.hpp"
#include "game/game_systems.hpp"
#include "game/enemy_ai.hpp"
#include "game/level_editor.hpp"
#include "engine/ai/behavior_tree.hpp"
#include "engine/core/components.hpp"
#include "engine/effects/blood.hpp"
#include "engine/fog/fog_volume.hpp"
#include "engine/physics/collision.hpp"
#include "engine/particles/particles.hpp"
#include "engine/physics/physics.hpp"
#include "engine/rain/nuradhi_rain.hpp"
#include "engine/rain/rain_volume.hpp"
#include "engine/render/render_components.hpp"
#include "engine/snow/snow_volume.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <imgui.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

//std
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace game {

	using namespace nrd;
	using ecs::Entity;

	namespace {
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
			input.bindAction("toggleEditor", Key::F2);
			input.bindAction("toggleNoir", Key::F3);
			input.bindAction("toggleCrt", Key::F4);
			input.bindAction("weapon1", Key::Num1); //Summers: melee, uzis, katanas
			input.bindAction("weapon2", Key::Num2);
			input.bindAction("weapon3", Key::Num3);
			input.bindAction("pickup", Key::E);
			input.bindAction("pause", Key::P); //pauses and resumes
			input.bindAction("restart", Key::R); //after a game over
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
				registry.add<Name>(light, Name{ "Light " + std::to_string(i + 1) });
			}
		}
	}

	namespace {
		//Weather: each kind is a particle volume that follows the player along x, so it's always on screen
		void spawnWeather(ecs::Registry& registry, Entity player)
		{
			const auto add = [&](const char* name, const ParticleSettings& settings, const glm::vec3& position, bool enabled) {
				const Entity weather = registry.create();
				registry.add<Transform>(weather, Transform{ position });
				ParticleEmitter& emitter = registry.add<ParticleEmitter>(weather, ParticleEmitter{ settings });
				emitter.enabled = enabled;
				emitter.randomState ^= static_cast<std::uint32_t>(weather) * 2654435761u; //different random streams per emitter
				registry.add<CameraFollow>(weather, CameraFollow{ player, { true, false, false }, glm::vec3{ 0.f } });
				registry.add<Name>(weather, Name{ name });
			};
			//rain and snow fill the air from above the camera's view down to the floor (y = 1.5)
			add("Particle rain", ParticleSettings::rain(), { 1.f, 0.2f, 3.f }, false);
			add("Particle snow", ParticleSettings::snow(), { 1.f, 0.2f, 3.f }, false);
			//the original particle fog hugs the ground too; kept, but the volumetric fog below replaces it by default
			add("Particle fog", ParticleSettings::fog(), { 1.f, 1.38f, 3.f }, false);

			//volumetric fog rising from the floor, which the fighters push, part and stir as they move and fight
			const Entity fog = registry.create();
			registry.add<Transform>(fog, Transform{ { 1.f, 1.5f, 3.f } });
			registry.add<FogVolume>(fog, FogVolume::realistic());
			registry.add<CameraFollow>(fog, CameraFollow{ player, { true, false, false }, glm::vec3{ 0.f } });
			registry.add<Name>(fog, Name{ "Volumetric fog" });

			//volumetric snowfall around the fighters, off while it rains (switch it on in the level editor)
			const Entity snow = registry.create();
			registry.add<Transform>(snow, Transform{ { 1.f, 1.5f, 3.f } });
			registry.add<SnowVolume>(snow, SnowVolume::graphicNovel()).enabled = false;
			registry.add<CameraFollow>(snow, CameraFollow{ player, { true, false, false }, glm::vec3{ 0.f } });
			registry.add<Name>(snow, Name{ "Volumetric snow" });

			//volumetric rain that splashes and ripples on the floor; kept, but Nuradhi Rain below is the default
			const Entity rain = registry.create();
			registry.add<Transform>(rain, Transform{ { 1.f, 1.5f, 3.f } });
			registry.add<RainVolume>(rain, RainVolume::realisticRain()).enabled = false;
			registry.add<CameraFollow>(rain, CameraFollow{ player, { true, false, false }, glm::vec3{ 0.f } });
			registry.add<Name>(rain, Name{ "Volumetric rain" });

			//Nuradhi Rain: the particle rain, splashing and rippling exactly where each drop lands
			const Entity nuradhiRain = registry.create();
			registry.add<Transform>(nuradhiRain, Transform{ { 1.f, 0.2f, 3.f } }); //the volume's bottom is the floor (y = 1.5)
			registry.add<ParticleEmitter>(nuradhiRain, ParticleEmitter{ ParticleSettings::rain() }).randomState ^= static_cast<std::uint32_t>(nuradhiRain) * 2654435761u;
			registry.add<NuradhiRain>(nuradhiRain);
			registry.add<CameraFollow>(nuradhiRain, CameraFollow{ player, { true, false, false }, glm::vec3{ 0.f } });
			registry.add<Name>(nuradhiRain, Name{ "Nuradhi rain" });
		}
	}

	namespace {
		constexpr int ENEMY_WAVE_SIZE = 5; //enemies kept in the fight at once
		constexpr float ENEMY_RESPAWN_DELAY = 3.f; //seconds before a beaten enemy is replaced
		constexpr float FLOOR_EDGES_X[2] = { -4.5f, 4.5f }; //where the floors end along x, so nobody spawns off them

		//Makes enemies: a Blaze with a behaviour tree and her own temperament, so a crowd isn't a row of clones
		struct EnemyFactory {
			TextureHandle texture;
			std::shared_ptr<SpriteSheet> sheet;
			Entity target; //who they fight
			std::mt19937 random{ 777u };
			int made = 0;

			float roll() { return std::uniform_real_distribution<float>{ 0.f, 1.f }(random); }

			Entity spawn(ecs::Registry& registry, const glm::vec3& position)
			{
				const Entity enemy = archive::spawnBlaze(registry, texture, sheet, position, false);
				registry.get<CharacterAnimations>(enemy).faceMovement = false; //the tree turns her to face her target
				registry.add<Health>(enemy, Health{ 8, 8 });

				EnemyFighter fighter{ target };
				fighter.moveSpeed = 0.36f + 0.14f * roll();
				fighter.windup = 0.2f + 0.15f * roll();
				fighter.recoverTime = 0.55f + 0.35f * roll();
				fighter.kickChance = 0.25f + 0.2f * roll();
				planNextAttack(fighter);
				if (registry.findFirst<EnemySquad>() != ecs::NullEntity) {
					//with a squad she waits where she is until it sends her in; the first few turns are staggered
					fighter.role = EnemyFighter::Role::Waiter;
					fighter.slot = { position.x, position.z };
					fighter.cooldown = 0.3f + 1.2f * roll();
				}
				registry.add<EnemyFighter>(enemy, fighter);
				registry.add<BehaviorTree>(enemy, BehaviorTree{ makeFighterTree() });
				registry.add<Name>(enemy, Name{ "Enemy " + std::to_string(++made) });
				return enemy;
			}
		};
	}
	namespace {
		//A colour for ImGui text and draw lists. ImGui takes sRGB values, but the swapchain encodes on write, so they're
		//made linear first or they come out washed out (the style colours get the same treatment in the renderer).
		ImVec4 linearColour(float r, float g, float b, float a = 1.f)
		{
			return { std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f), a };
		}

		void spawnWave(ecs::Registry& registry, EnemyFactory& enemies, float centreX)
		{
			for (int i = 0; i < ENEMY_WAVE_SIZE; i++) {
				const float side = i % 2 == 0 ? -1.f : 1.f; //alternate sides, further out for each pair
				enemies.spawn(registry, { centreX + side * (2.4f + 0.8f * (i / 2)), 1.5f, 2.3f + 1.4f * enemies.roll() });
			}
		}

		//What the pause and game over screens need to know
		struct FightState {
			Entity player;
			glm::vec3 playerStart;
			std::shared_ptr<EnemyFactory> enemies;
			bool gameOver = false;
			float gameOverTime = 0.f; //seconds since she went down
		};

		//Starts the fight over: Summers back at the start with full health, bare handed; every enemy, bullet and drop of
		//blood gone; a fresh wave
		void restartFight(Engine& engine, FightState& state)
		{
			ecs::Registry& registry = engine.registry();
			std::vector<Entity> doomed; //removed after the loops: entities can't be removed while they're being iterated
			registry.each<EnemyFighter>([&](Entity entity, EnemyFighter&) { doomed.push_back(entity); });
			registry.each<Projectile>([&](Entity entity, Projectile&) { doomed.push_back(entity); });
			for (const Entity entity : doomed) registry.destroy(entity);
			registry.each<BloodEffect>([](Entity, BloodEffect& blood) {
				blood.droplets.clear();
				blood.splatsOnGround.clear();
			});
			registry.each<EnemySquad>([](Entity, EnemySquad& squad) { squad.gapTimer = 0.f; });

			registry.get<Transform>(state.player).position = state.playerStart;
			registry.get<RigidBody>(state.player).velocity = glm::vec3{ 0.f };
			Health& health = registry.get<Health>(state.player);
			health.hp = health.maxHp;
			health.hitStun = 0.f;
			Combat& combat = registry.get<Combat>(state.player);
			combat.currentMove.clear();
			combat.cooldowns.clear();
			combat.hitThisAttack.clear();
			combat.airAttackUsed = false;
			combat.timer = 0.f;
			registry.get<PlayerController>(state.player).sprintDirection = 0.f;
			setWeaponMode(registry.get<CharacterAnimations>(state.player), registry.get<AttackBindings>(state.player), registry.get<Weapons>(state.player), WeaponMode::Melee);

			spawnWave(registry, *state.enemies, state.playerStart.x);
			state.gameOver = false;
			engine.paused = false;
			if (engine.debugLog) std::cout << "fight restarted" << std::endl;
		}

		constexpr ImGuiWindowFlags OVERLAY_WINDOW = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

		//P pauses and resumes. When Summers goes down the game over screen takes over until she's restarted. This runs in the
		//Ui phase, which carries on while the game is paused.
		void gameFlow(Engine& engine, FightState& state, float dt)
		{
			ecs::Registry& registry = engine.registry();
			const Input& input = engine.input();
			const ImGuiViewport* viewport = ImGui::GetMainViewport();

			const Health* health = registry.tryGet<Health>(state.player);
			if (!state.gameOver && health && health->defeated()) {
				state.gameOver = true;
				state.gameOverTime = 0.f;
				if (engine.debugLog) std::cout << "game over" << std::endl;
				engine.paused = false; //the world carries on around her: the rain, the fog, the enemies standing over her
			}

			if (state.gameOver) {
				state.gameOverTime += dt;
				bool restart = state.gameOverTime > 1.f && input.actionPressed("restart"); //not before she's hit the ground
				const float fade = std::min(state.gameOverTime / 1.5f, 1.f);
				//dim the whole screen: a window the size of the screen that lets clicks and keys through
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(viewport->Size);
				ImGui::SetNextWindowBgAlpha(0.72f * fade);
				if (ImGui::Begin("##dim", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {}
				ImGui::End();
				if (state.gameOverTime > 0.8f) {
					ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, { 0.5f, 0.5f });
					ImGui::SetNextWindowBgAlpha(0.f);
					if (ImGui::Begin("##gameover", nullptr, OVERLAY_WINDOW)) {
						ImGui::PushFont(nullptr, 110.f);
						ImGui::TextColored(linearColour(0.85f, 0.08f, 0.08f), "GAME OVER");
						ImGui::PopFont();
						ImGui::PushFont(nullptr, 26.f);
						ImGui::TextColored(linearColour(0.8f, 0.8f, 0.84f), "Summers is down.");
						ImGui::Spacing();
						if (ImGui::Button("Try again  (R)", { 260.f, 52.f })) restart = true;
						ImGui::PopFont();
					}
					ImGui::End();
				}
				if (restart) restartFight(engine, state);
				return;
			}

			if (input.actionPressed("pause")) {
				engine.paused = !engine.paused;
				if (engine.debugLog) std::cout << (engine.paused ? "paused" : "resumed") << std::endl;
			}
			if (engine.paused) {
				ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, { 0.5f, 0.5f });
				ImGui::SetNextWindowBgAlpha(0.6f);
				if (ImGui::Begin("##paused", nullptr, OVERLAY_WINDOW)) {
					ImGui::PushFont(nullptr, 64.f);
					ImGui::TextColored(linearColour(0.92f, 0.92f, 0.95f), "PAUSED");
					ImGui::PopFont();
					ImGui::PushFont(nullptr, 22.f);
					ImGui::TextColored(linearColour(0.7f, 0.7f, 0.75f), "P to resume");
					ImGui::PopFont();
				}
				ImGui::End();
			}
		}
	}
	void loadTestLevel(Engine& engine)
	{
		ecs::Registry& registry = engine.registry();
		Renderer& renderer = engine.renderer();
		bindControls(engine.input());

		const TextureHandle logoTexture = renderer.loadTexture("Images/Sprites/Nuradhi Logo copy.png");
		const MeshHandle vaseMesh = renderer.loadMesh("models/flat_vase.obj");
		const MeshHandle quadMesh = renderer.loadMesh("models/quad.obj");

		spawnLights(registry);

		//Summers, the player: she starts bare handed and swaps to uzis or katanas with 1, 2 and 3
		//(shared with the editor's rebake button, which swaps a new atlas into it)
		const auto summersAssets = std::make_shared<SummersAssets>(loadSummers(renderer, engine.debugLog));
		const Entity player = spawnSummers(registry, *summersAssets, { 1.f, 1.5f, 3.f });

		//The enemies: Blaze, retired from the player's seat (see game/archive), run by behaviour trees. A squad decides
		//which of them attack and which hang back around Summers (see EnemySquad), so a crowd takes turns and flanks her
		//instead of queuing up. Fighters pass through each other, as in most beat 'em ups. The wave is topped up as
		//enemies are beaten, so there's always a fight.
		const Entity squad = registry.create();
		registry.add<EnemySquad>(squad);
		registry.add<Name>(squad, Name{ "Enemy squad" });

		const TextureHandle blazeTexture = renderer.loadTexture("Images/Sprites/Blaze.png", TextureFilter::Pixelated);
		auto enemies = std::make_shared<EnemyFactory>(EnemyFactory{ blazeTexture, archive::loadBlazeSheet(engine.debugLog), player });
		spawnWave(registry, *enemies, 1.f);
		auto fight = std::make_shared<FightState>(FightState{ player, { 1.f, 1.5f, 3.f }, enemies });
		//Colliders are in world units around each entity's position, and +y is down: a box with offset y = -0.25
		//and half height 0.25 sits on top of its position, one with offset y = +0.05 hangs just below it.

		//vase standing on the floor
		const Entity vase = registry.create();
		registry.add<Transform>(vase, Transform{ { 2.f, 1.5f, 1.f } });
		registry.add<MeshRenderer>(vase, MeshRenderer{ vaseMesh, logoTexture });
		registry.add<Collider>(vase, Collider::box({ 0.25f, 0.25f, 0.25f }, { 0.f, -0.25f, 0.f }));
		registry.add<Health>(vase, Health{ 2, 2 }); //low: only kicks reach it
		registry.add<Name>(vase, Name{ "Vase" });

		//two 10 x 2 floor slabs, 0.1 thick, whose top surface is at their position
		int floorNumber = 0;
		for (const float z : { 2.f, 4.f }) {
			const Entity floor = registry.create();
			registry.add<Transform>(floor, Transform{ { 0.f, 1.5f, z }, glm::vec3{ 0.f }, { 5.f, 1.f, 1.f } });
			registry.add<MeshRenderer>(floor, MeshRenderer{ quadMesh, logoTexture });
			registry.add<Collider>(floor, Collider::box({ 5.f, 0.05f, 1.f }, { 0.f, 0.05f, 0.f }));
			registry.add<EditorBounds>(floor, EditorBounds{ { -1.f, 0.f, -1.f }, { 1.f, 0.1f, 1.f } }); //the quad, 0.1 thick below it
			registry.add<Name>(floor, Name{ "Floor " + std::to_string(++floorNumber) });
		}

		spawnWeather(registry, player);

		//the uzis' bullets are streaks drawn by this emitter, whose particles projectileSystem remakes every frame
		const Entity tracers = registry.create();
		registry.add<Transform>(tracers, Transform{ { 0.f, 0.f, 0.f } });
		{
			ParticleSettings settings{};
			settings.maxParticles = 256;
			settings.size = { 0.02f, 0.02f };
			settings.colour = { 1.f, 0.85f, 0.45f, 0.95f }; //a hot yellow
			settings.stretch = 0.012f; //streaks about 0.14 units long at a bullet's speed
			settings.softness = 0.6f;
			settings.prewarm = false;
			ParticleEmitter& emitter = registry.add<ParticleEmitter>(tracers, ParticleEmitter{ settings });
			emitter.enabled = false; //nothing spawns by itself
		}
		registry.add<BulletTracers>(tracers);
		registry.add<Name>(tracers, Name{ "Bullet tracers" });

		//hits spray blood (see attackSystem); it stays red under the film noir filter only with keep red turned up
		const Entity blood = registry.create();
		registry.add<BloodEffect>(blood);
		registry.add<Name>(blood, Name{ "Blood" });

		//orthographic side view, following Summers along x, tilted down a little
		const Entity camera = registry.create();
		registry.add<Transform>(camera, Transform{ { 1.f - 0.4f, -0.75f, -3.5f }, { -0.3f, 0.f, 0.f } });
		registry.add<Camera>(camera);
		registry.add<CameraFollow>(camera, CameraFollow{ player, { true, false, false }, { -0.4f, 0.f, 0.f } });
		registry.add<CameraYawControl>(camera);

		//beaten enemies are replaced, one at a time, from whichever side has fewer, just off screen
		engine.addSystem(Phase::Update, [enemies, player, timer = 0.f](Engine& engine, float dt) mutable {
			ecs::Registry& registry = engine.registry();
			const Transform* playerTransform = registry.tryGet<Transform>(player);
			if (!playerTransform) return;
			int alive = 0, left = 0, right = 0;
			registry.each<EnemyFighter, Transform>([&](Entity, EnemyFighter&, Transform& transform) {
				alive++;
				(transform.position.x < playerTransform->position.x ? left : right)++;
			});
			if (alive >= ENEMY_WAVE_SIZE) {
				timer = 0.f;
				return;
			}
			timer += dt;
			if (timer < ENEMY_RESPAWN_DELAY) return;
			timer = 0.f;
			float side = left <= right ? -1.f : 1.f;
			float x = playerTransform->position.x + side * (3.2f + 0.6f * enemies->roll());
			if (x < FLOOR_EDGES_X[0] || x > FLOOR_EDGES_X[1]) { //no room on that side: the other one
				side = -side;
				x = playerTransform->position.x + side * 3.4f;
			}
			enemies->spawn(registry, { std::clamp(x, FLOOR_EDGES_X[0], FLOOR_EDGES_X[1]), 1.5f, 2.3f + 1.4f * enemies->roll() });
		});
		engine.addSystem(Phase::Update, enemySquadSystem); //after the spawner, before the trees run
		engine.addSystem(Phase::Ui, [fight](Engine& engine, float dt) { gameFlow(engine, *fight, dt); });
		engine.addSystem(Phase::Update, hitStunSystem);
		engine.addSystem(Phase::Update, weaponSystem);
		engine.addSystem(Phase::Update, playerAttackInputSystem);
		engine.addSystem(Phase::Update, playerControlSystem);
		engine.addSystem(Phase::Update, cameraYawSystem);
		engine.addSystem(Phase::Update, orbitSystem);
		engine.addSystem(Phase::Update, debugOverlayToggleSystem);
		engine.addSystem(Phase::Update, filterToggleSystem);
		engine.addSystem(Phase::LateUpdate, attackSystem);
		engine.addSystem(Phase::LateUpdate, projectileSystem);
		engine.addSystem(Phase::LateUpdate, characterAnimationSystem);

		EditorActions actions;
		actions.rebakeSprites = [&engine, summersAssets]() { return rebakeSummers(engine.renderer(), engine.registry(), *summersAssets); };
		addLevelEditor(engine, logoTexture, std::move(actions));
	}
}
