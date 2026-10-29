#include "engine/rain/rain_volume.hpp"

//std
#include <cmath>

namespace nrd {

	RainVolume RainVolume::cinematic()
	{
		return RainVolume{};
	}

	RainVolume RainVolume::realisticRain()
	{
		RainVolume rain{};
		rain.realistic = true;
		return rain;
	}

	glm::vec2 RainVolume::windVector() const
	{
		return glm::vec2{ std::cos(windDirection), std::sin(windDirection) } * windSpeed;
	}

	void updateRainVolumes(ecs::Registry& registry, float dt)
	{
		registry.each<RainVolume>([&](ecs::Entity, RainVolume& rain) {
			rain.time += dt;
		});
	}
}
