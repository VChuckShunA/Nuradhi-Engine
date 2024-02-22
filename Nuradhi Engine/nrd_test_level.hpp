#pragma once

#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_window.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"

#include <memory>
#include <vector>
#include<unordered_map>



namespace nrd {
	class TestLevel {
	public:
		static constexpr int WIDTH = 1920;
		static constexpr int HEIGHT = 1080;

		TestLevel();
		~TestLevel();

		//Delete these cuz we're using a pointer to our GLFW window
		TestLevel(const TestLevel&) = delete;
		TestLevel& operator=(const TestLevel&) = delete;

	

		void run();
	private:
		void loadGameObjects();

		lve::LveWindow lveWindow{ WIDTH,HEIGHT,"Nuradhi - A Film Noir Game Engine" };
		lve::LveDevice lveDevice{ lveWindow };
		lve::LveRenderer lveRenderer{ lveWindow, lveDevice };

		//note: order of declaration matters
		std::unique_ptr<lve::LveDescriptorPool> globalPool{};
		lve::LveGameObject::Map gameObjects;
	};
}