#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

namespace nrd {

	//Volumetric ground fog: a slab of animated noise that rises from the entity's Transform position (the ground,
	//+y is down) and is drawn as a stack of horizontal slices, so anything standing in it is fogged from the feet up.
	//Point lights glow inside it. The style settings turn it from smooth film fog into inked, halftoned comic fog.
	//The older particle fog (ParticleSettings::fog) is separate and still works alongside or instead of this.
	struct FogVolume {
		bool enabled = true;

		//shape
		glm::vec2 halfExtents{ 4.f, 1.8f }; //x and z around the entity; the edges fade out softly
		float height = 0.4f; //how far it rises above the ground
		int slices = 32; //more slices, smoother through anything standing in the fog (and more fill cost)

		//look
		glm::vec3 colour{ 0.72f, 0.74f, 0.78f }; //lit colour, linear
		float density = 2.2f; //extinction per unit at the ground; the noise and height falloff thin it
		float coverage = 0.45f; //0 is a solid blanket, towards 1 only the thickest wisps remain
		float heightFalloff = 2.5f; //how quickly it thins with height (1 is gentle, 5 hugs the ground)
		float noiseScale = 1.6f; //size of the billows: higher is smaller and busier
		glm::vec2 wind{ 0.12f, 0.03f }; //drift along x and z, units per second
		float turbulence = 0.6f; //how much the billows swirl and curl as they drift
		float shadow = 0.45f; //darkens the thin parts so the thick billows read as lit volume (0 flat, 1 strong)
		float lightScatter = 6.f; //how strongly point lights glow in the fog

		//graphic novel style
		int toneBands = 0; //> 0 posterises the fog into that many hard-edged tones, like inked cloud shapes
		float halftone = 0.f; //0..1 blends the fog into a printed halftone dot screen
		float halftoneSize = 7.f; //dot spacing in pixels

		float time = 0.f; //advanced by updateFogVolumes; stops while the game is paused

		//smooth, cinematic fog
		static FogVolume cinematic();
		//inked bands and halftone dots: a panel from a noir comic
		static FogVolume graphicNovel();
	};

	//Advances the fog's animation
	void updateFogVolumes(ecs::Registry& registry, float dt);
}
