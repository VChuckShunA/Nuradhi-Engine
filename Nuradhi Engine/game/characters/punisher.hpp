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

//Frank Castle, the Punisher: a player character cut from one arcade sprite sheet. He fights with his fists (left mouse
//punches, right mouse kicks, and both work in the air as a jump punch and a flying kick) and with his pistol, which he
//draws and keeps firing for as long as the special button (F) is held. He has no weapon swaps: the pistol is always on him.
namespace game {

	//Everything his sprites need: one texture holding every frame, and the sheet that names his animations
	struct PunisherAssets {
		nrd::TextureHandle texture;
		std::shared_ptr<nrd::SpriteSheet> sheet;
		float pixelsPerUnit = 1.f;
	};

	//Cuts his frames out of the big sheet (Images/Sprites/Arcade - The Punisher ...), packs them into one atlas, baked
	//after the first run, and names the animations
	PunisherAssets loadPunisher(nrd::Renderer& renderer, bool describe);

	//Throws away the baked atlas, builds it again from the sheet and swaps it into the running game (see rebakeSummers)
	std::string rebakePunisher(nrd::Renderer& renderer, nrd::ecs::Registry& registry, PunisherAssets& assets);

	//His moves: "punch", "kick", "jumpPunch", "jumpKick", "special" (the pistol) and "jumpSpecial"; the names the default
	//AttackBindings use
	Combat punisherCombat();

	//The player: the Punisher at `position` (his feet)
	nrd::ecs::Entity spawnPunisher(nrd::ecs::Registry& registry, const PunisherAssets& assets, const glm::vec3& position);
}
