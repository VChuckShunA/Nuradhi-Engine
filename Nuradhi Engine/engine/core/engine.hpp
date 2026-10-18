#pragma once

#include "engine/ecs/registry.hpp"
#include "engine/input/input.hpp"
#include "engine/physics/physics.hpp"
#include "engine/render/renderer.hpp"

//std
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lve {
	class LveWindow;
}

namespace nrd {

	struct EngineConfig {
		int width = 1280;
		int height = 720;
		std::string title = "Nuradhi";
	};

	//When a game system runs within a frame:
	//  Update      game logic and input (set velocities, jump, ...)
	//  [ai]        engine: every BehaviorTree is ticked
	//  [physics]   engine: gravity, movement, collision
	//  LateUpdate  reactions to physics (pick animations from grounded, ...)
	//  [engine]    sprite animation, camera follow, render
	enum class Phase { Update, LateUpdate };

	//Owns the window, input, renderer and the entity registry, and runs the frame loop. Game code builds its
	//scene through registry() and renderer(), adds systems, then calls run().
	class Engine {
	public:
		using System = std::function<void(Engine& engine, float dt)>;

		explicit Engine(const EngineConfig& config);
		~Engine();

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		ecs::Registry& registry() { return world; }
		Input& input() { return *keyboard; }
		Renderer& renderer() { return *graphics; }

		void addSystem(Phase phase, System system);
		void run(); //until the window closes

		PhysicsSettings physics{};
		bool debugLog = false; //prints collision contacts every frame (and whatever else games choose to log)

	private:
		//declaration order matters: the renderer and input use the window, so it's created first and destroyed last
		std::unique_ptr<lve::LveWindow> window;
		std::unique_ptr<Input> keyboard;
		std::unique_ptr<Renderer> graphics;
		ecs::Registry world;
		std::vector<System> updateSystems;
		std::vector<System> lateUpdateSystems;
	};
}
