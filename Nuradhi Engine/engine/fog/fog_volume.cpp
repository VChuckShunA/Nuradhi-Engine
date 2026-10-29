#include "engine/fog/fog_volume.hpp"
#include "engine/core/components.hpp"
#include "engine/fog/fog_simulation.hpp"

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

	FogVolume FogVolume::realistic()
	{
		FogVolume fog{};
		fog.density = 3.f; //a thicker blanket, so the gaps characters leave in it read clearly
		fog.coverage = 0.3f;
		fog.turbulence = 0.45f;
		fog.wind = { 0.06f, 0.02f };
		fog.slices = 40;
		fog.interactive = true;
		return fog;
	}

	void updateFogVolumes(ecs::Registry& registry, float dt)
	{
		registry.each<FogVolume, Transform>([&](ecs::Entity, FogVolume& fog, Transform& transform) {
			fog.time += dt;
			if (!fog.enabled || !fog.interactive) return;
			if (!fog.simulation) fog.simulation = std::make_shared<FogSimulation>();
			fog.simulation->step(registry, transform.position, fog, dt);
		});
	}
}
