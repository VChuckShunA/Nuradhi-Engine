#pragma once

#include "game/game_components.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/render/render_components.hpp"
#include "engine/sprite/sprite.hpp"

//std
#include <memory>

//Blaze, the game's first player character, now retired from the player's seat and kept here whole: her sprite sheet
//and animations, her attacks, and a function that puts her in the world. She's still the sparring partner (the dummy
//run by a behaviour tree), and spawnBlaze(..., asPlayer = true) makes her the player again.
namespace game::archive {

	constexpr float BLAZE_PIXELS_PER_UNIT = 75.f; //sheet pixels per world unit: her ~75 px idle pose stands 1 unit tall, like her collider

	//her sheet: frames found from Images/Sprites/Blaze.png, and the animations that pick which to play
	std::shared_ptr<nrd::SpriteSheet> loadBlazeSheet(bool describe);

	//her attacks: punch, kick, jumpPunch, jumpKick, special, jumpSpecial (the AttackBindings defaults)
	Combat blazeCombat();

	//Adds Blaze's sprite, body, animations and attacks to a new entity at `position` (her feet) and returns it. With
	//asPlayer she also gets the controls and a health bar that doesn't run out.
	nrd::ecs::Entity spawnBlaze(nrd::ecs::Registry& registry, nrd::TextureHandle texture, const std::shared_ptr<nrd::SpriteSheet>& sheet,
		const glm::vec3& position, bool asPlayer);
}
