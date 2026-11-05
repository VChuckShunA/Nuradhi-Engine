#include "engine/effects/lightning.hpp"
#include "engine/core/components.hpp"
#include "engine/effects/blood.hpp"

//std
#include <algorithm>
#include <cmath>#include <vector>

namespace nrd {

	namespace {
		//xorshift, like the other effects: small, fast, and the state lives in the component
		float random01(std::uint32_t& state)
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return static_cast<float>(state >> 8) * (1.f / 16777216.f);
		}

		float randomRange(std::uint32_t& state, float min, float max) { return min + (max - min) * random01(state); }

		constexpr float RAIN_NEEDED = 0.05f; //how hard it has to rain for onlyInRain strikes to come

		//One stroke from `from` to `to`, broken in halves again and again, each midpoint pushed sideways: that's what
		//makes it jagged. Some midpoints throw off a fork (thinner, fainter, shorter), which breaks up the same way.
		std::size_t pointsSoFar(const Lightning& lightning)
		{
			std::size_t total = 0;
			for (const Lightning::Stroke& stroke : lightning.bolt) total += stroke.points.size();
			return total;
		}

		//Turns a run of positions into a stroke and files it. The channel thins out and, for a fork, fades towards its end.
		void finishStroke(Lightning& lightning, const std::vector<glm::vec3>& positions, float width, float brightness, bool fork)
		{
			if (positions.size() < 2) return;
			float total = 0.f;
			for (std::size_t i = 1; i < positions.size(); i++) total += glm::length(positions[i] - positions[i - 1]);
			const float endWidth = fork ? 0.25f : 0.7f;
			const float endBrightness = fork ? 0.25f : 1.f;

			Lightning::Stroke stroke;
			stroke.fork = fork;
			float travelled = 0.f;
			for (std::size_t i = 0; i < positions.size(); i++) {
				if (i > 0) travelled += glm::length(positions[i] - positions[i - 1]);
				const float t = total > 0.f ? travelled / total : 0.f;
				stroke.points.push_back({ positions[i], width * (1.f + (endWidth - 1.f) * t), brightness * (1.f + (endBrightness - 1.f) * t) });
			}
			lightning.bolt.push_back(std::move(stroke));
		}

		//Breaks the way from `out`'s last point to `to` in halves again and again, each midpoint pushed sideways: that's
		//what makes it jagged. Some midpoints throw off a fork (thinner, fainter, shorter), which breaks up the same way.
		void breakUp(Lightning& lightning, std::vector<glm::vec3>& out, const glm::vec3& to, float width, float brightness, int levels, float jitter)
		{
			const glm::vec3 from = out.back();
			if (levels == 0) {
				out.push_back(to);
				return;
			}
			std::uint32_t& random = lightning.randomState;
			const glm::vec3 along = to - from;
			const float length = glm::length(along);
			if (length < 1e-5f) {
				out.push_back(to);
				return;
			}
			const glm::vec3 side = glm::vec3{ -along.y, along.x, 0.f } / length; //across the stroke, on screen
			const glm::vec3 middle = (from + to) * 0.5f + side * randomRange(random, -1.f, 1.f) * length * jitter;

			breakUp(lightning, out, middle, width, brightness, levels - 1, jitter);

			//a fork leaves some of the middle joints, leaning away from the stroke and carrying on downwards
			if (levels >= 2 && width > lightning.boltWidth * 0.3f && pointsSoFar(lightning) + out.size() + 70 < Lightning::MAX_POINTS
				&& random01(random) < lightning.branches * 0.45f) {
				const float lean = randomRange(random, 0.35f, 0.9f) * (random01(random) < 0.5f ? -1.f : 1.f);
				const float cosine = std::cos(lean);
				const float sine = std::sin(lean);
				glm::vec3 direction{ along.x * cosine - along.y * sine, along.x * sine + along.y * cosine, 0.f };
				if (direction.y < 0.f) direction = -direction; //forks go down
				const glm::vec3 end = middle + direction * randomRange(random, 0.6f, 1.1f);
				std::vector<glm::vec3> fork{ middle };
				breakUp(lightning, fork, end, width * 0.55f, brightness * 0.6f, levels - 2, jitter * 1.2f);
				finishStroke(lightning, fork, width * 0.55f, brightness * 0.6f, true);
			}

			breakUp(lightning, out, to, width, brightness, levels - 1, jitter);
		}

		void buildBolt(Lightning& lightning, const Transform& transform)
		{
			std::uint32_t& random = lightning.randomState;
			lightning.bolt.clear();
			const float x = transform.position.x + randomRange(random, -lightning.halfWidth, lightning.halfWidth);
			const float z = transform.position.z + lightning.depth;
			const glm::vec3 top{ x, transform.position.y - lightning.height, z };
			const glm::vec3 ground{ x + randomRange(random, -0.5f, 0.5f), transform.position.y + 0.3f, z }; //it lands in the haze at the far end of the street
			std::vector<glm::vec3> main{ top };
			breakUp(lightning, main, ground, lightning.boltWidth, 1.f, 6, 0.16f);
			finishStroke(lightning, main, lightning.boltWidth, 1.f, false);
		}

		//Two to four stutters, a quick run of weak ones building to the big one, then a slow afterglow
		void buildPulses(Lightning& lightning)
		{
			std::uint32_t& random = lightning.randomState;
			lightning.pulses.clear();
			const int stutters = 2 + static_cast<int>(random01(random) * 2.99f);
			const int main = stutters - 1 - static_cast<int>(random01(random) * 0.99f); //usually the last, sometimes the one before
			float start = 0.f;
			for (int i = 0; i < stutters; i++) {
				const bool big = i == main;
				lightning.pulses.push_back({ start, big ? 1.f : randomRange(random, 0.35f, 0.65f), big ? randomRange(random, 0.12f, 0.2f) : randomRange(random, 0.03f, 0.06f) });
				start += randomRange(random, 0.05f, 0.13f);
			}
			lightning.pulses.push_back({ lightning.pulses[main].start + 0.05f, 0.28f, 0.55f }); //the sky stays lit a moment longer
		}

		float pulseBrightness(const Lightning::Pulse& pulse, float age)
		{
			const float since = age - pulse.start;
			if (since < 0.f) return 0.f;
			const float rise = std::clamp(since / 0.012f, 0.f, 1.f); //a hard, near-instant onset
			return pulse.peak * rise * std::exp(-since / pulse.decay);
		}

		void strike(Lightning& lightning, const Transform& transform)
		{
			buildBolt(lightning, transform);
			buildPulses(lightning);
			lightning.age = 0.f;
		}
	}

	void strikeLightning(ecs::Registry& registry, ecs::Entity entity)
	{
		Lightning* lightning = registry.tryGet<Lightning>(entity);
		const Transform* transform = registry.tryGet<Transform>(entity);
		if (lightning && transform) strike(*lightning, *transform);
	}

	LightningFlash currentLightning(ecs::Registry& registry)
	{
		LightningFlash result;
		registry.each<Lightning>([&](ecs::Entity, Lightning& lightning) {
			if (lightning.flash <= result.brightness) return;
			result.brightness = lightning.flash;
			result.colour = lightning.colour;
			result.sceneLight = lightning.sceneLight;
			result.skyGlow = lightning.skyGlow;
		});
		return result;
	}

	void updateLightning(ecs::Registry& registry, float dt)
	{
		const float rain = rainIntensity(registry);
		registry.each<Lightning, Transform>([dt, rain](ecs::Entity, Lightning& lightning, Transform& transform) {
			if (!lightning.enabled) {
				lightning.age = -1.f;
				lightning.flash = 0.f;
				lightning.bolt.clear();
				return;
			}

			if (lightning.age < 0.f) {
				lightning.flash = 0.f;
				lightning.timer -= dt;
				if (lightning.timer > 0.f) return;
				if (lightning.onlyInRain && rain < RAIN_NEEDED) {
					lightning.timer = 0.5f; //the sky waits for rain
					return;
				}
				strike(lightning, transform);
				lightning.timer = lightning.meanInterval * randomRange(lightning.randomState, 0.5f, 1.5f);
			}
			else {
				lightning.age += dt;
			}

			float brightness = 0.f;
			float lastLight = 0.f;
			for (const Lightning::Pulse& pulse : lightning.pulses) {
				brightness = std::max(brightness, pulseBrightness(pulse, lightning.age));
				lastLight = std::max(lastLight, pulse.start + pulse.decay * 5.f);
			}
			lightning.flash = brightness * lightning.flashStrength;
			if (lightning.age > lastLight) {
				lightning.age = -1.f;
				lightning.flash = 0.f;
				lightning.pulses.clear();
				lightning.bolt.clear();
			}
		});
	}
}
