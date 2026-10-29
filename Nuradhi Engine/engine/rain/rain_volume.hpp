#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

namespace nrd {

	//Volumetric rainfall: layers of procedural streaks at different depths around the entity's Transform position
	//(the ground at the middle of the volume, +y is down). Every layer faces the camera and is depth tested, so rain
	//falls in front of and behind whoever stands in it. Near layers have longer, faster, brighter drops for parallax,
	//the fall comes in gusting sheets, a fine mist hangs low, and the drops glint where point lights catch them.
	//Realistic mode adds what happens at the ground: splashes where each drop lands, and ripples spreading across
	//the floor.
	//The older particle rain (ParticleSettings::rain) is separate and still works alongside or instead of this.
	struct RainVolume {
		bool enabled = true;

		//shape (the defaults cover the same space as the particle rain)
		float halfWidth = 3.5f; //across the screen, around the entity; the sides fade out
		float height = 2.6f; //how far the rainfall reaches above the ground
		float depth = 1.5f; //layers spread this far in front of and behind the entity
		int layers = 10; //more layers, a deeper, heavier fall (and more fill cost)

		//droplets (the defaults match the particle rain's drops, about 900 of them)
		float fallSpeed = 8.f; //units per second at the focus depth
		float dropSize = 0.0028f; //half thickness of a streak (the particle rain's drops, about 0.0055 across)
		float dropLength = 0.2f; //length of a streak: a drop smeared by the shutter
		float spacing = 0.05f; //one drop at most per column of this width; smaller is heavier rain
		float density = 0.13f; //0..1 chance that a cell holds a drop
		float depthScaling = 0.f; //0..1: near layers' drops bigger, longer, faster and brighter than far ones (1 is 0.55x to 1.4x); 0 draws every drop alike, like the particle rain
		float tailFade = 0.f; //0..1: 0 is an evenly bright streak with soft ends, like the particle rain; 1 is bright at the leading end, fading up the tail

		//wind
		float windSpeed = 0.65f; //units per second
		float windDirection = 3.14159265f; //radians around the vertical, 0 blows towards +x (screen right), pi towards -x
		float gusts = 0.5f; //0..1: the rain comes in sheets that sweep through with the wind

		//look
		glm::vec4 colour{ 0.75f, 0.78f, 0.85f, 0.35f }; //lit colour (linear) and drop opacity
		float depthOfField = 0.003f; //softening per unit of distance from the focus depth
		float focus = 0.f; //depth of the sharpest drops, relative to the entity
		float mist = 0.12f; //fine spray hanging low over the ground
		float lightScatter = 6.f; //how brightly drops and ripples glint near point lights

		//realistic mode: the rain meets the ground
		bool realistic = false;
		float splashes = 1.f; //0..1 brightness of the crowns thrown up where drops land
		float rippleRate = 0.5f; //ripples per second per patch of floor
		float rippleSize = 0.075f; //widest a ripple spreads, units
		float rippleLife = 0.7f; //seconds a ripple lasts
		float rippleStrength = 0.35f; //0..1 brightness of the rings

		float time = 0.f; //advanced by updateRainVolumes; stops while the game is paused

		//film rain: long, soft streaks in gusting sheets
		static RainVolume cinematic();
		//cinematic rain that splashes and ripples where it lands
		static RainVolume realisticRain();

		//the wind as a horizontal vector (x, z), units per second
		glm::vec2 windVector() const;
	};

	//Advances the rainfall
	void updateRainVolumes(ecs::Registry& registry, float dt);
}
