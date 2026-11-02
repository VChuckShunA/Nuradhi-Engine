#include "engine/effects/blood.hpp"
#include "engine/particles/particles.hpp"
#include "engine/rain/nuradhi_rain.hpp"
#include "engine/rain/rain_volume.hpp"

//std
#include <algorithm>
#include <cmath>

namespace nrd {

	namespace {
		//xorshift, like the particle emitters: small, fast, and the state lives in the component
		float random01(std::uint32_t& state)
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return static_cast<float>(state >> 8) * (1.f / 16777216.f);
		}

		float randomRange(std::uint32_t& state, float min, float max) { return min + (max - min) * random01(state); }
	}

	void spawnBlood(ecs::Registry& registry, const glm::vec3& at, const glm::vec3& direction, float amount, float groundY)
	{
		const ecs::Entity entity = registry.findFirst<BloodEffect>();
		if (entity == ecs::NullEntity || amount <= 0.f) return;
		BloodEffect& blood = registry.get<BloodEffect>(entity);
		std::uint32_t& random = blood.randomState;

		const glm::vec3 aim = glm::length(direction) > 1e-4f ? glm::normalize(direction) : glm::vec3{ 1.f, -0.4f, 0.f };
		const int count = std::max(1, static_cast<int>(std::round(blood.dropletsPerHit * amount)));
		for (int i = 0; i < count && blood.droplets.size() < BloodEffect::MAX_DROPLETS; i++) {
			//a cone around the hit direction: a little in depth too, so it lands across the floor rather than in a line
			const float yaw = randomRange(random, -blood.spread, blood.spread);
			const float pitch = randomRange(random, -blood.spread, blood.spread * 0.6f);
			glm::vec3 dir = aim;
			dir.y += std::sin(pitch); //+y is down: negative pitch throws it up
			dir.z += std::sin(yaw) * 0.6f;
			dir = glm::normalize(dir);
			const float speed = blood.spraySpeed * randomRange(random, 0.35f, 1.f);
			blood.droplets.push_back({ at, dir * speed, blood.dropletSize * randomRange(random, 0.5f, 1.3f), groundY, 0.f });
		}
	}

	float rainIntensity(ecs::Registry& registry)
	{
		float rain = 0.f;
		registry.each<ParticleEmitter>([&](ecs::Entity entity, ParticleEmitter& emitter) {
			if (!emitter.enabled) return;
			//Nuradhi Rain, or any emitter whose drops fall fast: that's rain (snow drifts down, fog and bullets don't fall)
			if (registry.tryGet<NuradhiRain>(entity) || emitter.settings.velocityMin.y > 4.f) rain = std::max(rain, emitter.density);
		});
		registry.each<RainVolume>([&](ecs::Entity, RainVolume& volume) {
			if (volume.enabled) rain = std::max(rain, volume.density / 0.13f); //0.13 is the density that matches the particle rain
		});
		return std::clamp(rain, 0.f, 1.5f);
	}

	void updateBlood(ecs::Registry& registry, float dt)
	{
		const float rain = rainIntensity(registry);
		registry.each<BloodEffect>([dt, rain](ecs::Entity, BloodEffect& blood) {
			std::uint32_t& random = blood.randomState;
			for (BloodEffect::Droplet& droplet : blood.droplets) {
				droplet.velocity.y += blood.gravity * dt;
				droplet.position += droplet.velocity * dt;
				droplet.age += dt;
				if (droplet.position.y < droplet.groundY || droplet.velocity.y <= 0.f) continue;

				//landed: maybe a splat, sized by the drop
				if (blood.splats && random01(random) < blood.splatChance) {
					if (blood.splatsOnGround.size() >= BloodEffect::MAX_SPLATS) blood.splatsOnGround.erase(blood.splatsOnGround.begin());
					const float size = blood.splatSize * std::clamp(droplet.size / blood.dropletSize, 0.3f, 1.3f) * randomRange(random, 0.6f, 1.f);
					blood.splatsOnGround.push_back({ { droplet.position.x, droplet.groundY, droplet.position.z }, size, randomRange(random, 0.f, 100.f), 0.f });
				}
				droplet.age = -1.f; //gone
			}
			std::erase_if(blood.droplets, [](const BloodEffect::Droplet& droplet) { return droplet.age < 0.f || droplet.age > 3.f; });

			const float lifetime = blood.splatLife + blood.splatFade;
			const float wash = rain * blood.rainWash * dt;
			for (BloodEffect::Splat& splat : blood.splatsOnGround) {
				splat.age += dt;
				splat.washed = std::min(1.f, splat.washed + wash);
			}
			std::erase_if(blood.splatsOnGround, [lifetime](const BloodEffect::Splat& splat) { return splat.age >= lifetime || splat.washed >= 1.f; });
		});
	}
}
