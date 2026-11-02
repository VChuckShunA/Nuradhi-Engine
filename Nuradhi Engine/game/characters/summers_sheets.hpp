#pragma once

#include "engine/sprite/sprite_atlas.hpp"

//std
#include <vector>

namespace game {

	//World units a relaxed standing frame of Summers is tall (hair to shoes); her collider is a little shorter
	constexpr float SUMMERS_STANDING_HEIGHT = 1.05f;

	//Every sprite sheet Summers uses, in "mode.animation" names, with how each one is scaled and cleaned
	std::vector<nrd::SpriteSource> summersSheets();

	//Where the barrels of her guns are on an atlas frame, as points in pixels from the frame's top-left corner: the tips of
	//the near-black guns that stick out furthest forward (she faces right), one per gun, top to bottom. Empty if the frame
	//has no gun pointing forward. The hair is black too, but it trails behind her, and the muzzle flashes are bright.
	std::vector<glm::vec2> gunMuzzles(const nrd::SpriteAtlas& atlas, int frameIndex);
}
