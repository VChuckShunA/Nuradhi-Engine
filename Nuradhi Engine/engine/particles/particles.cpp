#include "engine/particles/particles.hpp"
#include "engine/core/components.hpp"

//libs
#include <glm/gtc/constants.hpp>

//std
#include <algorithm>
#include <cmath>

namespace nrd {

	ParticleSettings ParticleSettings::rain()
	{
		ParticleSettings rain{};
		rain.volumeHalfExtents = { 3.5f, 1.3f, 1.5f };
		rain.maxParticles = 900;
		rain.spawnRate = 3000.f;
		rain.velocityMin = { -0.8f, 7.f, 0.f }; //falling fast (+y is down), slanted by the wind
		rain.velocityMax = { -0.5f, 9.f, 0.f };
		rain.size = { 0.004f, 0.007f };
		rain.colour = { 0.75f, 0.78f, 0.85f, 0.35f };
		rain.stretch = 0.025f; //streaks about 0.2 units long
		rain.softness = 0.6f;
		rain.wrap = true;
		return rain;
	}

	ParticleSettings ParticleSettings::snow()
	{
		ParticleSettings snow{};
		snow.volumeHalfExtents = { 3.5f, 1.3f, 1.5f };
		snow.maxParticles = 600;
		snow.spawnRate = 2000.f;
		snow.velocityMin = { -0.15f, 0.35f, -0.05f };
		snow.velocityMax = { 0.15f, 0.6f, 0.05f };
		snow.size = { 0.02f, 0.045f };
		snow.colour = { 0.95f, 0.95f, 1.f, 0.85f };
		snow.softness = 0.55f;
		snow.sway = { 0.25f, 0.6f }; //flakes drift side to side as they fall
		snow.wrap = true;
		return snow;
	}

	ParticleSettings ParticleSettings::fog()
	{
		ParticleSettings fog{};
		fog.volumeHalfExtents = { 4.f, 0.12f, 1.8f }; //a low bank: put the emitter just above the ground
		fog.maxParticles = 160;
		fog.spawnRate = 14.f;
		fog.lifetime = { 9.f, 14.f };
		fog.velocityMin = { 0.08f, -0.01f, -0.02f }; //rolling slowly along
		fog.velocityMax = { 0.2f, 0.01f, 0.02f };
		fog.size = { 0.45f, 0.9f }; //big, overlapping puffs
		fog.colour = { 0.8f, 0.82f, 0.85f, 0.06f }; //each one faint; the overlaps build the density
		fog.fadeIn = 2.f;
		fog.fadeOut = 3.f;
		fog.stretch = 4.f; //drawn out along the drift into flat, wide banks
		fog.softness = 1.f;
		fog.wrap = true;
		return fog;
	}

	namespace {
		//xorshift: small and fast, and keeps each emitter's randomness in its own component
		float random01(std::uint32_t& state)
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return static_cast<float>(state >> 8) * (1.f / 16777216.f);
		}

		float randomRange(std::uint32_t& state, float min, float max) { return min + (max - min) * random01(state); }

		glm::vec3 randomRange(std::uint32_t& state, const glm::vec3& min, const glm::vec3& max)
		{
			return { randomRange(state, min.x, max.x), randomRange(state, min.y, max.y), randomRange(state, min.z, max.z) };
		}

		//weather that wraps and never fades lives until the emitter is switched off
		bool immortal(const ParticleSettings& settings)
		{
			return settings.wrap && settings.fadeIn <= 0.f && settings.fadeOut <= 0.f;
		}

		Particle spawn(ParticleEmitter& emitter, const glm::vec3& centre, bool randomAge)
		{
			const ParticleSettings& settings = emitter.settings;
			std::uint32_t& random = emitter.randomState;
			Particle particle{};
			particle.position = centre + randomRange(random, -settings.volumeHalfExtents, settings.volumeHalfExtents);
			particle.velocity = randomRange(random, settings.velocityMin, settings.velocityMax);
			particle.lifetime = randomRange(random, settings.lifetime.x, settings.lifetime.y);
			particle.size = randomRange(random, settings.size.x, settings.size.y);
			particle.phase = randomRange(random, 0.f, glm::two_pi<float>());
			//prewarmed particles start part way through their lives, so they don't all fade in together
			if (randomAge) particle.age = randomRange(random, 0.f, particle.lifetime);
			return particle;
		}
	}

	void updateParticles(ecs::Registry& registry, float dt)
	{
		registry.each<Transform, ParticleEmitter>([dt](ecs::Entity, Transform& transform, ParticleEmitter& emitter) {
			const ParticleSettings& settings = emitter.settings;
			const glm::vec3 centre = transform.position;
			const glm::vec3 half = settings.volumeHalfExtents;
			const std::size_t capacity = static_cast<std::size_t>(std::max(0.f, settings.maxParticles * emitter.density));
			emitter.impacts.clear();

			if (emitter.enabled && settings.prewarm && !emitter.prewarmed) {
				while (emitter.particles.size() < capacity) emitter.particles.push_back(spawn(emitter, centre, true));
				emitter.prewarmed = true;
			}
			if (emitter.particles.size() > capacity) emitter.particles.resize(capacity); //density was turned down

			if (emitter.enabled) {
				emitter.spawnBudget += settings.spawnRate * emitter.density * dt;
				while (emitter.spawnBudget >= 1.f && emitter.particles.size() < capacity) {
					emitter.particles.push_back(spawn(emitter, centre, false));
					emitter.spawnBudget -= 1.f;
				}
				if (emitter.particles.size() >= capacity) emitter.spawnBudget = 0.f;
			}

			const bool forever = immortal(settings);
			for (std::size_t i = 0; i < emitter.particles.size();) {
				Particle& particle = emitter.particles[i];
				particle.age += dt;
				bool dead = !forever && particle.age >= particle.lifetime;

				if (!dead) {
					glm::vec3 velocity = particle.velocity;
					if (settings.sway.x != 0.f) {
						velocity.x += settings.sway.x * std::sin(glm::two_pi<float>() * settings.sway.y * particle.age + particle.phase);
					}
					particle.position += velocity * dt;

					if (settings.wrap) {
						for (int axis = 0; axis < 3; axis++) {
							const float offset = particle.position[axis] - centre[axis];
							if (std::abs(offset) <= half[axis]) continue;
							//out through the bottom: it hit the ground where it crossed it this frame
							if (axis == 1 && offset > 0.f && emitter.recordImpacts && velocity.y > 0.f) {
								emitter.impacts.push_back(particle.position - velocity * ((offset - half.y) / velocity.y));
							}
							//a switched-off emitter lets its weather leave instead of recycling it
							if (!emitter.enabled) {
								dead = true;
								break;
							}
							particle.position[axis] -= std::copysign(2.f * half[axis], offset);
						}
					}
				}

				if (dead) {
					emitter.particles[i] = emitter.particles.back();
					emitter.particles.pop_back();
				}
				else {
					i++;
				}
			}
		});
	}
}
