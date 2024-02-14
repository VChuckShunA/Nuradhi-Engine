#pragma once

#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_window.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"

#include <memory>
#include <vector>


namespace lve {
	class FirstApp {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		FirstApp();
		~FirstApp();

		//Delete these cuz we're using a pointer to our GLFW window
		FirstApp(const FirstApp&) = delete;
		FirstApp& operator=(const FirstApp&) = delete;

		void run();
	private:
		void loadGameObjects();
		
		LveWindow lveWindow{ WIDTH,HEIGHT,"Welcome Vulkan" };
		LveDevice lveDevice{ lveWindow };
		LveRenderer lveRenderer{ lveWindow, lveDevice };

		//note: order of declaration matters
		std::unique_ptr<LveDescriptorPool> globalPool{};
		LveGameObject::Map gameObjects;
	};
}