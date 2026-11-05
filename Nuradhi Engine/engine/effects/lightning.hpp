#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>
#include <vector>

namespace nrd {

	//Component: lightning. Put one on an entity with a Transform (its position is the ground under the sky it strikes,
	//+y is down; give it a CameraFollow on x so the storm follows the fight). Every few seconds, while it rains, a
	//strike comes: a jagged bolt tears down the sky behind the stage and the whole scene is flashed cold white in
	//two to four hard stutters, the biggest in the middle, the way a strike looks in an old noir film. The flash lights
	//the scene (sprites, meshes), lifts the sky and drives the film filter harder; the bolt is a real forked mesh of
	//ribbons, so it goes through the filters with everything else.
	struct Lightning {
		bool enabled = true;

		//when
		float meanInterval = 9.f; //seconds between strikes, on average (each wait is 0.5x to 1.5x of it)
		bool onlyInRain = true; //strikes wait for rain: any Nuradhi Rain, rain volume or fast-falling particle emitter that is on

		//the flash
		float flashStrength = 1.f; //0..2 how bright the biggest stutter of a strike is
		float sceneLight = 1.1f; //ambient light a full flash adds to everything lit (the world is dim: this makes it bright for an instant)
		float skyGlow = 0.16f; //0..1 how far the dark sky lifts towards the flash colour at full flash (gamma space)
		glm::vec3 colour{ 0.78f, 0.86f, 1.f }; //linear, a cold blue-white

		//the bolt
		bool showBolt = true;
		float halfWidth = 3.2f; //strikes land this far either side of the entity, across the screen
		float height = 3.6f; //how far up the bolt starts, above the entity
		float depth = 2.8f; //how far behind the entity the bolt is drawn (+z is away from the camera)
		float boltWidth = 0.014f; //thickness of the main stroke
		float glow = 5.f; //the halo around the bolt, as a multiple of its thickness
		float branches = 0.6f; //0..1 how many forks it throws off

		//state (the engine runs it)
		struct Point {
			glm::vec3 position{ 0.f };
			float width = 0.01f; //thickness here: forks and the foot of the bolt thin out
			float brightness = 1.f; //forks are fainter than the main stroke and die away along their length
		};
		struct Stroke { //one unbroken run of the bolt, drawn as a single strip so there are no seams at the joints
			std::vector<Point> points;
			bool fork = false; //a fork starts inside its parent, so it needs no rounded start
		};
		struct Pulse {
			float start = 0.f; //seconds after the strike begins
			float peak = 1.f; //0..1 brightness it reaches at its start
			float decay = 0.1f; //seconds for it to fall to a third
		};
		float timer = 4.f; //seconds until the next strike (the first comes early)
		float age = -1.f; //seconds since the strike began; negative when none is under way
		std::vector<Pulse> pulses;
		std::vector<Stroke> bolt;
		float flash = 0.f; //current brightness, 0..1.5 with the strength applied
		std::uint32_t randomState = 0x51A9E3D7u;

		static constexpr std::size_t MAX_POINTS = 400; //across the whole bolt
	};

	//What the renderer needs from every Lightning at once
	struct LightningFlash {
		float brightness = 0.f; //0 when there is no flash
		glm::vec3 colour{ 1.f };
		float sceneLight = 0.f; //ambient to add at full brightness
		float skyGlow = 0.f;
	};
	LightningFlash currentLightning(ecs::Registry& registry);

	//Starts a strike now, in a random place across the sky (what the editor's button does). Does nothing if the entity
	//has no Lightning and Transform.
	void strikeLightning(ecs::Registry& registry, ecs::Entity entity);

	//Counts down to strikes, plays the stutters and keeps the flash brightness up to date
	void updateLightning(ecs::Registry& registry, float dt);
}
