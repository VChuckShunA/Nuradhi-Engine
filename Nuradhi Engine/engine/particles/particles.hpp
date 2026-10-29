#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>
#include <vector>

namespace nrd {

	//How an emitter's particles are born, move and look. Units are world units and seconds; +y is down.
	struct ParticleSettings {
		glm::vec3 volumeHalfExtents{ 2.f, 1.f, 1.f }; //particles are born anywhere in this box around the emitter
		int maxParticles = 500;
		float spawnRate = 100.f; //particles per second, until maxParticles are alive
		glm::vec2 lifetime{ 2.f, 4.f }; //seconds, min..max; ignored when wrap is on and fadeIn/fadeOut are 0
		glm::vec3 velocityMin{ 0.f };
		glm::vec3 velocityMax{ 0.f };
		glm::vec2 size{ 0.02f, 0.04f }; //diameter, min..max
		glm::vec4 colour{ 1.f, 1.f, 1.f, 0.5f }; //rgb and peak alpha
		float fadeIn = 0.f; //seconds to fade up after birth
		float fadeOut = 0.f; //seconds to fade away before death
		float stretch = 0.f; //> 0 stretches particles along their velocity: length = size + speed * stretch (rain streaks)
		float softness = 1.f; //edge falloff: 0 is a hard disc, 1 a gradient all the way from the centre (fog)
		glm::vec2 sway{ 0.f }; //sideways drift: amplitude (units per second) and frequency (Hz), e.g. snow
		//particles that leave the volume come back in on the opposite side and live forever, so weather never
		//runs dry and the volume can follow the camera
		bool wrap = false;
		bool prewarm = true; //fill the volume straight away instead of starting empty

		static ParticleSettings rain();
		static ParticleSettings snow();
		static ParticleSettings fog();
	};

	struct Particle {
		glm::vec3 position{ 0.f };
		glm::vec3 velocity{ 0.f };
		float age = 0.f;
		float lifetime = 1.f;
		float size = 0.03f;
		float phase = 0.f; //random offset for sway
	};

	//Component: a particle system centred on the entity's Transform position
	struct ParticleEmitter {
		ParticleSettings settings{};
		bool enabled = true; //off stops spawning; live particles finish their lives
		float density = 1.f; //scales maxParticles and spawnRate, e.g. light rain vs a downpour

		std::vector<Particle> particles;
		float spawnBudget = 0.f;
		std::uint32_t randomState = 0x9E3779B9u;
		bool prewarmed = false;
	};

	//spawns, moves and retires particles; the engine runs it every frame (not while paused)
	void updateParticles(ecs::Registry& registry, float dt);
}
