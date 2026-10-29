#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

namespace nrd {

	//Volumetric snowfall: layers of procedural flakes at different depths around the entity's Transform position
	//(the ground at the middle of the volume, +y is down). Every layer faces the camera and is depth tested, so
	//snow falls in front of and behind whoever stands in it. Near layers have bigger, faster flakes for parallax,
	//flakes away from the focus depth are defocused, a faint snow mist hangs between the layers, and point lights
	//glow on the flakes. The style settings turn it from soft film snow into crisp, inked comic snow.
	//The older particle snow (ParticleSettings::snow) is separate and still works alongside or instead of this.
	struct SnowVolume {
		bool enabled = true;

		//shape
		float halfWidth = 4.5f; //across the screen, around the entity; the sides fade out
		float height = 3.5f; //how far the snowfall reaches above the ground
		float depth = 2.f; //layers spread this far in front of and behind the entity
		int layers = 12; //more layers, a deeper, denser fall (and more fill cost)

		//flakes
		float flakeSize = 0.0045f; //radius at the focus depth; near flakes are bigger, far ones smaller
		float spacing = 0.16f; //one flake at most per cell of this size; smaller is a heavier fall
		float density = 0.35f; //0..1 chance that a cell holds a flake
		float fallSpeed = 0.45f; //units per second at the focus depth
		float wind = 0.12f; //drift across the screen, units per second
		glm::vec2 sway{ 0.05f, 0.4f }; //side-to-side flutter: amplitude (units) and frequency (Hz)
		float motionBlur = 0.25f; //stretches flakes along their fall

		//look
		glm::vec4 colour{ 0.9f, 0.92f, 0.96f, 0.9f }; //lit colour (linear) and flake opacity
		float depthOfField = 0.004f; //blur per unit of distance from the focus depth; 0 keeps every flake sharp
		float focus = 0.f; //depth of the sharpest flakes, relative to the entity (0 is where the fighters stand)
		float mist = 0.2f; //haze of distant snow between the layers
		float lightScatter = 4.f; //how strongly point lights glow on the flakes and mist

		//graphic novel style
		float outline = 0.f; //0..1: an ink ring around each flake, strongest on near flakes
		int toneBands = 0; //> 0 posterises the mist into that many hard-edged tones
		float halftone = 0.f; //0..1 prints the mist as a halftone dot screen
		float halftoneSize = 7.f; //dot spacing in pixels

		float time = 0.f; //advanced by updateSnowVolumes; stops while the game is paused

		//soft, defocused film snow
		static SnowVolume cinematic();
		//crisp, inked flakes over a halftone mist: a panel from a noir comic
		static SnowVolume graphicNovel();
	};

	//Advances the snowfall
	void updateSnowVolumes(ecs::Registry& registry, float dt);
}
