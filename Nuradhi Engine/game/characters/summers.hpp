#pragma once

#include "game/game_components.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/render/render_components.hpp"
#include "engine/sprite/sprite.hpp"

//std
#include <memory>
#include <string>

namespace nrd {
	class Renderer;
}

//Summers, the player. She fights three ways, switched with the 1, 2 and 3 keys (see weaponSystem): bare handed, with
//dual uzis, and with dual katanas. Every mode has the same animations to draw on, named "<mode>.<what>": idle, walk,
//run, jump, hurt and pickup, plus the mode's own attacks.
namespace game {

	//Everything her sprites need: one texture holding every frame of every sheet, and the sheet that names her animations
	struct SummersAssets {
		nrd::TextureHandle texture;
		std::shared_ptr<nrd::SpriteSheet> sheet;
		float pixelsPerUnit = 1.f;
	};

	//Loads her 29 sprite sheets, cleans their backgrounds, scales them to match and packs them into one atlas (takes a
	//moment; it happens once at startup), then names the animations
	SummersAssets loadSummers(nrd::Renderer& renderer, bool describe);

	//Throws away the baked atlas, builds it again from her PNGs and swaps it into the running game: the texture is replaced
	//in place and every sprite of hers keeps playing what it was playing. Takes a few seconds, and the game stands still
	//meanwhile. Throws if a sheet can't be read, with everything left as it was. Returns a line saying what it did.
	std::string rebakeSummers(nrd::Renderer& renderer, nrd::ecs::Registry& registry, SummersAssets& assets);

	//Every attack of every mode, and the weapon swaps (see weaponSystem for how they're started)
	Combat summersCombat();

	//Switches the animations and the attack buttons to `mode`'s, without any swap animation
	void setWeaponMode(CharacterAnimations& animations, AttackBindings& bindings, Weapons& weapons, WeaponMode mode);
	const char* weaponName(WeaponMode mode); //"melee", "uzi" or "katana"

	//The player: Summers at `position` (her feet), in melee
	nrd::ecs::Entity spawnSummers(nrd::ecs::Registry& registry, const SummersAssets& assets, const glm::vec3& position);
}
