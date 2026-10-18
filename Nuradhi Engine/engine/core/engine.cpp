#include "engine/core/engine.hpp"
#include "engine/ai/behavior_tree.hpp"
#include "engine/core/components.hpp"
#include "engine/platform/lve_window.hpp"
#include "engine/sprite/sprite.hpp"

//std
#include <chrono>

namespace nrd {

	Engine::Engine(const EngineConfig& config)
		: window{ std::make_unique<lve::LveWindow>(config.width, config.height, config.title) },
		keyboard{ std::make_unique<Input>(*window) },
		graphics{ std::make_unique<Renderer>(*window) }
	{
	}

	Engine::~Engine()
	{
		//the registry is destroyed after the renderer, but components only hold handles, so nothing on the GPU is left behind
		if (graphics) graphics->waitIdle();
	}

	void Engine::addSystem(Phase phase, System system)
	{
		(phase == Phase::Update ? updateSystems : lateUpdateSystems).push_back(std::move(system));
	}

	void Engine::run()
	{
		auto previousTime = std::chrono::high_resolution_clock::now();
		while (!window->shouldClose()) {
			glfwPollEvents();
			const auto now = std::chrono::high_resolution_clock::now();
			const float dt = std::chrono::duration<float, std::chrono::seconds::period>(now - previousTime).count();
			previousTime = now;
			keyboard->update(dt);

			for (System& system : updateSystems) system(*this, dt);
			updateBehaviorTrees(world, dt); //AI decides like a player would: after input, before physics
			stepPhysics(world, dt, physics, debugLog);
			for (System& system : lateUpdateSystems) system(*this, dt);
			updateSpriteAnimations(world, dt);
			updateCameraFollow(world);

			graphics->render(world, dt);
		}
		//the CPU blocks until all GPU work is done
		graphics->waitIdle();
	}
}
