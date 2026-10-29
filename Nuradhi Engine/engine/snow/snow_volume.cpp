#include "engine/snow/snow_volume.hpp"

namespace nrd {

	SnowVolume SnowVolume::cinematic()
	{
		return SnowVolume{};
	}

	SnowVolume SnowVolume::graphicNovel()
	{
		SnowVolume snow{};
		snow.flakeSize = 0.0055f;
		snow.colour = { 0.96f, 0.96f, 0.97f, 1.f }; //paper white
		snow.motionBlur = 0.f;
		snow.depthOfField = 0.f; //comic flakes are drawn, not photographed: every one is sharp
		snow.mist = 0.3f;
		snow.outline = 0.35f;
		snow.toneBands = 3;
		snow.halftone = 0.6f;
		return snow;
	}

	void updateSnowVolumes(ecs::Registry& registry, float dt)
	{
		registry.each<SnowVolume>([&](ecs::Entity, SnowVolume& snow) {
			snow.time += dt;
		});
	}
}
