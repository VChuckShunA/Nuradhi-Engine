#include "engine/fog/fog_volume.hpp"

namespace nrd {

	FogVolume FogVolume::cinematic()
	{
		return FogVolume{};
	}

	FogVolume FogVolume::graphicNovel()
	{
		FogVolume fog{};
		fog.colour = { 0.85f, 0.86f, 0.88f }; //paper white against the inked night
		fog.density = 2.8f;
		fog.coverage = 0.5f;
		fog.shadow = 0.6f;
		fog.toneBands = 4;
		fog.halftone = 0.55f;
		return fog;
	}

	void updateFogVolumes(ecs::Registry& registry, float dt)
	{
		registry.each<FogVolume>([&](ecs::Entity, FogVolume& fog) {
			fog.time += dt;
		});
	}
}
