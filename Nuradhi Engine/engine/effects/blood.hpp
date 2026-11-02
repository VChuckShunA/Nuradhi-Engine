#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>
#include <vector>

namespace nrd {

	//Component: blood. Put one on an entity of its own; spawnBlood sprays droplets from a hit, they arc and fall
	//under gravity, and the ones that reach the ground leave splats that stay a long while, until rain washes them away. The colour is
	//a strongly saturated red, so the film noir filter's keep red decides whether it stays red (the graphic novel
	//look) or turns black like the blood in an old noir film.
	struct BloodEffect {
		glm::vec3 colour{ 0.42f, 0.f, 0.01f }; //linear; keep it saturated so keep red can pick it out

		//spray
		int dropletsPerHit = 26; //for a hit of amount 1
		float spraySpeed = 2.2f; //units per second
		float spread = 0.7f; //radians either side of the hit direction
		float dropletSize = 0.012f; //diameter
		float gravity = 9.8f;

		//splats on the floor
		bool splats = true;
		float splatChance = 0.5f; //share of the droplets that land as a visible splat
		float splatSize = 0.045f; //largest splat diameter
		float splatLife = 120.f; //seconds a splat stays before fading, if the rain doesn't wash it away first
		float splatFade = 2.f; //seconds it takes to fade
		//Rain: any rain washes splats away (the Nuradhi Rain, the volumetric rain, or a particle emitter whose drops fall fast).
		//This is how much of a splat a full downpour washes off every second, so 0.1 takes about ten seconds; lighter rain
		//takes longer, and 0 makes blood proof against it.
		float rainWash = 0.1f;

		struct Droplet {
			glm::vec3 position;
			glm::vec3 velocity;
			float size = 0.01f;
			float groundY = 0.f; //it lands when it falls to this height (+y is down)
			float age = 0.f;
		};
		struct Splat {
			glm::vec3 position; //on the ground
			float size = 0.04f;
			float seed = 0.f; //shapes its outline
			float age = 0.f;
			float washed = 0.f; //0 untouched, 1 gone: the rain thins it, spreads it and washes it away
		};
		std::vector<Droplet> droplets;
		std::vector<Splat> splatsOnGround;
		std::uint32_t randomState = 0xB10D5EEDu;

		static constexpr std::size_t MAX_DROPLETS = 2000;
		static constexpr std::size_t MAX_SPLATS = 400; //the oldest go first
	};

	//Sprays blood from `at`, mostly along `direction` (it's normalised; give it an upward part for an arc), on to a
	//floor at `groundY`. `amount` scales how much: 1 is a normal hit. Does nothing without a BloodEffect entity.
	void spawnBlood(ecs::Registry& registry, const glm::vec3& at, const glm::vec3& direction, float amount, float groundY);

	//How hard it's raining, 0 for dry, 1 for the usual downpour: the strongest of every rain that's switched on. Used to
	//wash blood away; weather code can use it too.
	float rainIntensity(ecs::Registry& registry);

	//Moves the droplets, turns landed ones into splats, ages the splats and lets the rain wash them away; the engine runs it each frame
	void updateBlood(ecs::Registry& registry, float dt);
}
