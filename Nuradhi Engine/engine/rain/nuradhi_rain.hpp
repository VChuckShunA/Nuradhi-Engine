#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>
#include <vector>

namespace nrd {

	//Nuradhi Rain: the particle rain (a ParticleEmitter on the same entity draws the drops) with what happens when
	//they land. Every drop that falls out through the bottom of the emitter's volume has hit the ground there, and
	//some of those impacts throw up a splash crown and spread a ripple across the floor, exactly where the drop
	//landed. The drop controls below drive the emitter's settings, so the rain is tuned in one place.
	struct NuradhiRain {
		//drops (applied to the entity's ParticleEmitter)
		float fallSpeed = 8.f; //units per second
		float dropSize = 0.0055f; //thickness of a drop
		float dropLength = 0.2f; //length of a streak: a drop smeared by the shutter
		float windSpeed = 0.65f; //units per second
		float windDirection = 3.14159265f; //radians around the vertical, 0 blows towards +x (screen right), pi towards -x

		//ripples spreading across the floor
		bool ripples = true;
		float rippleChance = 0.12f; //share of the drops that leave a visible ripple
		float rippleSize = 0.075f; //widest a ripple spreads, units
		float rippleLife = 0.7f; //seconds a ripple lasts
		float rippleStrength = 0.4f; //0..1 brightness of the rings

		//splash crowns where drops land
		bool splashes = true;
		float splashChance = 0.12f; //share of the drops that throw up a visible splash
		int splashDroplets = 4; //droplets in a crown
		float splashSpeed = 0.7f; //how fast the droplets are thrown, units per second
		float splashSize = 0.007f; //diameter of a droplet
		float splashStrength = 0.85f; //0..1 brightness of the droplets

		glm::vec3 colour{ 0.75f, 0.78f, 0.85f }; //water, lit (linear)
		float lightGlint = 6.f; //how brightly ripples and splashes catch nearby point lights

		struct Ripple {
			glm::vec3 position; //centre, on the ground
			float age = 0.f;
		};
		struct Droplet {
			glm::vec3 position;
			glm::vec3 velocity;
			float groundY = 0.f; //the droplet is gone when it falls back to this height
			float age = 0.f;
		};
		std::vector<Ripple> activeRipples;
		std::vector<Droplet> activeDroplets;
		std::uint32_t randomState = 0x6C8E9CF5u;
		glm::vec4 appliedDrops{ -1.f }; //the drop controls last written to the emitter, to spot changes
		float appliedWind = -1.f;

		static constexpr std::size_t MAX_RIPPLES = 1500;
		static constexpr std::size_t MAX_DROPLETS = 2500;

		//the wind as a horizontal vector (x, z), units per second
		glm::vec2 windVector() const;
	};

	//Applies the drop controls to the rain's ParticleEmitter, then turns this frame's impacts into ripples and
	//splashes and ages them. Runs after updateParticles.
	void updateNuradhiRain(ecs::Registry& registry, float dt);
}
