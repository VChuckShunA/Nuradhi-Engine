#include "engine/rain/nuradhi_rain.hpp"
#include "engine/particles/particles.hpp"

//libs
#include <glm/gtc/constants.hpp>

//std
#include <algorithm>
#include <cmath>

namespace nrd {

	namespace {
		constexpr float GRAVITY = 9.8f; //splash droplets fall back under it (+y is down)

		//xorshift, like the particle emitters: small, fast, and the state lives in the component
		float random01(std::uint32_t& state)
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return static_cast<float>(state >> 8) * (1.f / 16777216.f);
		}

		float randomRange(std::uint32_t& state, float min, float max) { return min + (max - min) * random01(state); }

		//writes the drop controls into the emitter's settings, the same spreads the particle rain preset uses
		void applyDrops(const NuradhiRain& rain, ParticleSettings& settings)
		{
			const glm::vec2 wind = rain.windVector();
			const float speed = std::max(rain.fallSpeed, 0.1f);
			settings.velocityMin = { wind.x - 0.15f, speed * 0.875f, wind.y - 0.05f };
			settings.velocityMax = { wind.x + 0.15f, speed * 1.125f, wind.y + 0.05f };
			settings.size = { rain.dropSize * 0.73f, rain.dropSize * 1.27f };
			settings.stretch = std::max(rain.dropLength - rain.dropSize, 0.f) / speed; //streak length = size + speed * stretch
		}
	}

	glm::vec2 NuradhiRain::windVector() const
	{
		return glm::vec2{ std::cos(windDirection), std::sin(windDirection) } * windSpeed;
	}

	void updateNuradhiRain(ecs::Registry& registry, float dt)
	{
		registry.each<NuradhiRain, ParticleEmitter>([dt](ecs::Entity, NuradhiRain& rain, ParticleEmitter& emitter) {
			emitter.recordImpacts = true;
			std::uint32_t& random = rain.randomState;

			//1. the drop controls changed: update the settings, and the drops already falling, which keep the
			//velocity and size they were born with
			const glm::vec4 drops{ rain.fallSpeed, rain.dropSize, rain.dropLength, rain.windSpeed };
			if (drops != rain.appliedDrops || rain.windDirection != rain.appliedWind) {
				applyDrops(rain, emitter.settings);
				const ParticleSettings& settings = emitter.settings;
				for (Particle& particle : emitter.particles) {
					particle.velocity = {
						randomRange(random, settings.velocityMin.x, settings.velocityMax.x),
						randomRange(random, settings.velocityMin.y, settings.velocityMax.y),
						randomRange(random, settings.velocityMin.z, settings.velocityMax.z) };
					particle.size = randomRange(random, settings.size.x, settings.size.y);
				}
				rain.appliedDrops = drops;
				rain.appliedWind = rain.windDirection;
			}

			//2. what landed this frame
			for (const glm::vec3& impact : emitter.impacts) {
				if (rain.ripples && rain.activeRipples.size() < NuradhiRain::MAX_RIPPLES && random01(random) < rain.rippleChance) {
					rain.activeRipples.push_back({ impact, 0.f });
				}
				if (rain.splashes && random01(random) < rain.splashChance) {
					for (int i = 0; i < rain.splashDroplets && rain.activeDroplets.size() < NuradhiRain::MAX_DROPLETS; i++) {
						//a crown: thrown up and out, all the way round
						const float angle = randomRange(random, 0.f, glm::two_pi<float>());
						const float out = rain.splashSpeed * randomRange(random, 0.25f, 0.6f);
						const float up = rain.splashSpeed * randomRange(random, 0.7f, 1.2f);
						rain.activeDroplets.push_back({ impact, { std::cos(angle) * out, -up, std::sin(angle) * out }, impact.y, 0.f });
					}
				}
			}

			//3. age them
			const float rippleLife = std::max(rain.rippleLife, 0.05f);
			for (NuradhiRain::Ripple& ripple : rain.activeRipples) ripple.age += dt;
			std::erase_if(rain.activeRipples, [rippleLife](const NuradhiRain::Ripple& ripple) { return ripple.age >= rippleLife; });

			for (NuradhiRain::Droplet& droplet : rain.activeDroplets) {
				droplet.velocity.y += GRAVITY * dt;
				droplet.position += droplet.velocity * dt;
				droplet.age += dt;
			}
			std::erase_if(rain.activeDroplets, [](const NuradhiRain::Droplet& droplet) {
				return droplet.age > 1.f || (droplet.velocity.y > 0.f && droplet.position.y >= droplet.groundY);
			});
		});
	}
}
