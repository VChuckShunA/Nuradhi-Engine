#include "game/characters/summers.hpp"
#include "game/characters/summers_sheets.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/collision.hpp"
#include "engine/physics/physics.hpp"
#include "engine/render/renderer.hpp"
#include "engine/sprite/sprite_atlas.hpp"

//std
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace game {

	using namespace nrd;

	namespace {
		//An animation made of some of a sheet's frames, by column. The sheets are the sprite sources in summersSheets().
		void animate(SpriteSheet& sheet, const SpriteAtlas& atlas, const std::string& name, const std::string& source,
			const std::vector<int>& columns, float framesPerSecond, bool loop)
		{
			SpriteAnimation animation{ name, {}, framesPerSecond, loop };
			for (const int column : columns) animation.frames.push_back(atlas.frame(source, column));
			sheet.addAnimation(animation);
		}

		//every frame of a sheet, in order
		std::vector<int> all(const SpriteAtlas& atlas, const std::string& source)
		{
			std::vector<int> columns;
			for (int i = 0; i < atlas.frameCount(source); i++) columns.push_back(i);
			return columns;
		}

		//The animations every mode has: idle and walk loop, the run is faster, a jump rises then holds the fall, a hit
		//flinches back, and the pickup crouches down and stands again. Which frames are which comes from looking at the
		//sheets: the jump sheets open with a stand and a crouch before the take-off, which a jump here skips.
		void commonAnimations(SpriteSheet& sheet, const SpriteAtlas& atlas, const std::string& mode)
		{
			animate(sheet, atlas, mode + ".idle", mode + ".idle", all(atlas, mode + ".idle"), 6.f, true);
			animate(sheet, atlas, mode + ".walk", mode + ".walk", all(atlas, mode + ".walk"), 6.f, true);
			animate(sheet, atlas, mode + ".run", mode + ".run", all(atlas, mode + ".run"), 9.f, true);
			animate(sheet, atlas, mode + ".jump", mode + ".jump", { 2, 3, 4, 5 }, 8.f, false);
			animate(sheet, atlas, mode + ".hurt", mode + ".hurt", { 1, 2, 3, 4, 5 }, 16.f, false);
			animate(sheet, atlas, mode + ".pickup", mode + ".pickup", all(atlas, mode + ".pickup"), 10.f, false);
		}

		//An attack that doesn't hit anything: swapping weapons and picking things up. It locks her in place like any move.
		AttackMove idleMove(const std::string& animation, float frames, float framesPerSecond)
		{
			AttackMove move{};
			move.animation = animation;
			move.duration = frames / framesPerSecond;
			move.activeStart = move.activeEnd = 100.f; //never live
			move.damage = 0;
			return move;
		}
	}

	namespace {
		constexpr const char* SUMMERS_BAKE = "Images/Sprites/Summers/baked/summers.atlas";

		//Her atlas, with her animations and the guns' muzzle points on its sheet. The first run builds it from the 29 sheets
		//and saves it; later runs read the saved file back. ``rebuild`` throws the saved file away first.
		SpriteAtlas makeSummersAtlas(bool describe, bool rebuild)
		{
			SpriteAtlasSettings settings{};
			settings.describe = describe;
			if (rebuild) {
				std::error_code error;
				std::filesystem::remove(SUMMERS_BAKE, error);
			}
			SpriteAtlas atlas = loadOrBuildSpriteAtlas(summersSheets(), SUMMERS_STANDING_HEIGHT, settings, SUMMERS_BAKE);
			if (describe) std::cout << "Summers' atlas is " << atlas.width << "x" << atlas.height << " with " << atlas.sheet->frames().size() << " frames\n";
			SpriteSheet& sheet = *atlas.sheet;
			for (const char* mode : { "melee", "uzi", "katana" }) commonAnimations(sheet, atlas, mode);

			//--- melee: she opens in a guard, so the punch and kick start a frame or two in ---
			animate(sheet, atlas, "melee.punch", "melee.punch", { 1, 2, 3, 4, 5, 6, 7 }, 24.f, false); //guard, wind up, jab out on 4 and 5, pull back
			animate(sheet, atlas, "melee.kick", "melee.kick", { 2, 3, 4, 5, 6, 7, 8, 10 }, 22.f, false); //knee up, chamber, kick out on 6, recover
			animate(sheet, atlas, "melee.jumpPunch", "melee.jumpPunch", { 3, 4, 5, 6, 7 }, 10.f, false); //rising guard, punch on 5, held through the fall
			animate(sheet, atlas, "melee.jumpKick", "melee.jumpKick", { 2, 3, 4, 5, 6, 7 }, 11.f, false); //tuck, flying kick out on 5, held

			//--- uzis: she summons them in a flash of cyan; going back to melee plays that in reverse ---
			animate(sheet, atlas, "uzi.equip", "uzi.equip", all(atlas, "uzi.equip"), 12.f, false);
			animate(sheet, atlas, "uzi.unequip", "uzi.equip", { 7, 6, 5, 4, 3, 2, 1, 0 }, 14.f, false);
			//raise the guns, aim, and flash: the animation plays once and holds its last frame, the firing pose, for as long as the
			//button is held; letting go lowers them (see AttackMove::holdAt)
			animate(sheet, atlas, "uzi.fire", "uzi.fire", { 1, 2, 3, 4 }, 16.f, false);
			animate(sheet, atlas, "uzi.lower", "uzi.fire", { 6, 7 }, 16.f, false);
			animate(sheet, atlas, "uzi.jumpFire", "uzi.jumpFire", { 2, 3, 4, 5, 6, 7 }, 12.f, false); //rising, firing at the top, held through the fall

			//--- katanas ---
			animate(sheet, atlas, "katana.equip", "katana.equip", all(atlas, "katana.equip"), 10.f, false);
			animate(sheet, atlas, "katana.unequip", "katana.equip", { 7, 6, 5, 4, 3, 2, 1, 0 }, 12.f, false);
			animate(sheet, atlas, "katana.attack", "katana.attack", { 1, 2, 3, 4, 10 }, 18.f, false); //lunge, a sweeping slash, the cross cut, recover
			animate(sheet, atlas, "katana.heavy", "katana.attack", { 6, 7, 8, 9, 10 }, 12.f, false); //spin, overhead, downward cut, thrust, recover
			animate(sheet, atlas, "katana.airAttack", "katana.airAttack", { 3, 4, 5, 6, 7, 8 }, 12.f, false); //rising, swords up, a spinning slash, held

			//where each gun's barrel is on every frame of the firing sheets, so bullets leave from the guns as drawn (see attackSystem)
			for (const char* source : { "uzi.fire", "uzi.jumpFire" }) {
				for (int column = 0; column < atlas.frameCount(source); column++) {
					const int frame = atlas.frame(source, column);
					sheet.setAnchors(frame, "muzzle", gunMuzzles(atlas, frame));
				}
			}

			return atlas;
		}
	}

	SummersAssets loadSummers(Renderer& renderer, bool describe)
	{
		const auto started = std::chrono::steady_clock::now();
		SpriteAtlas atlas = makeSummersAtlas(describe, false);
		const auto atlasBuilt = std::chrono::steady_clock::now();
		SummersAssets assets;
		assets.texture = renderer.loadTextureFromPixels(atlas.pixels.data(), atlas.width, atlas.height, TextureFilter::Smooth);
		assets.sheet = atlas.sheet;
		assets.pixelsPerUnit = atlas.pixelsPerUnit;
		const auto uploaded = std::chrono::steady_clock::now();
		std::cout << "Summers: atlas " << (atlas.fromCache ? "loaded from the baked file in " : "built and baked in ") << std::chrono::duration<double>(atlasBuilt - started).count() << " s, texture " << std::chrono::duration<double>(uploaded - atlasBuilt).count() << " s" << std::endl;
		return assets;
	}

	std::string rebakeSummers(Renderer& renderer, ecs::Registry& registry, SummersAssets& assets)
	{
		const auto started = std::chrono::steady_clock::now();
		//Each animator points into its sheet's animations, which are about to be replaced, so note what every Summers sprite
		//is playing now. Nothing here changes until the new atlas has been built, so a failure leaves the old one in place.
		std::vector<std::pair<ecs::Entity, std::string>> playing;
		registry.each<Sprite>([&](ecs::Entity entity, Sprite& sprite) {
			if (&sprite.animator.sheet() == assets.sheet.get()) playing.push_back({ entity, sprite.animator.currentAnimation() });
		});

		SpriteAtlas atlas = makeSummersAtlas(false, true);
		renderer.replaceTextureFromPixels(assets.texture, atlas.pixels.data(), atlas.width, atlas.height, TextureFilter::Smooth);
		*assets.sheet = std::move(*atlas.sheet); //the same object, so every sprite sees the new frames
		assets.pixelsPerUnit = atlas.pixelsPerUnit;
		for (const auto& [entity, animation] : playing) {
			Sprite& sprite = registry.get<Sprite>(entity);
			sprite.pixelsPerUnit = assets.pixelsPerUnit;
			if (!animation.empty()) sprite.animator.play(animation, true);
		}
		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
		return "Rebaked Summers' sprites in " + std::to_string(seconds).substr(0, 4) + " s (" + std::to_string(assets.sheet->frames().size()) + " frames).";
	}
	Combat summersCombat()
	{
		//Positions are world units from her feet as if she faces right; -y is up. She's about 1.05 tall, her hands at
		//chest height around y = -0.72 and her feet near y = -0.1.
		Combat combat{};

		//--- melee ---
		AttackMove punch{};
		punch.animation = "melee.punch";
		punch.duration = 7.f / 24.f;
		punch.activeStart = 3.f / 24.f; //the jab is out on the 4th and 5th frames
		punch.activeEnd = 5.f / 24.f;
		punch.hitboxOffset = { 0.42f, -0.72f, 0.f };
		punch.hitboxHalfExtents = { 0.18f, 0.12f, 0.25f };
		punch.damage = 1;
		punch.knockback = 0.5f;
		combat.moves["melee.punch"] = punch;

		AttackMove kick{};
		kick.animation = "melee.kick";
		kick.duration = 8.f / 22.f;
		kick.activeStart = 4.f / 22.f; //the leg is out on the 5th and 6th frames
		kick.activeEnd = 6.f / 22.f;
		kick.hitboxOffset = { 0.6f, -0.6f, 0.f };
		kick.hitboxHalfExtents = { 0.28f, 0.16f, 0.25f };
		kick.damage = 2;
		kick.knockback = 1.f;
		combat.moves["melee.kick"] = kick;

		//jump attacks hold their pose and stay live until she lands (a jump lasts about 0.7 s)
		AttackMove jumpPunch{};
		jumpPunch.animation = "melee.jumpPunch";
		jumpPunch.duration = 0.6f;
		jumpPunch.activeStart = 0.18f; //the punch is out on the 3rd frame
		jumpPunch.activeEnd = 0.6f;
		jumpPunch.hitboxOffset = { 0.45f, -0.75f, 0.f };
		jumpPunch.hitboxHalfExtents = { 0.25f, 0.18f, 0.25f };
		jumpPunch.damage = 1;
		jumpPunch.knockback = 0.8f;
		jumpPunch.airborne = true;
		combat.moves["melee.jumpPunch"] = jumpPunch;

		AttackMove jumpKick{};
		jumpKick.animation = "melee.jumpKick";
		jumpKick.duration = 0.6f;
		jumpKick.activeStart = 0.25f; //the kick is out on the 4th frame
		jumpKick.activeEnd = 0.6f;
		jumpKick.hitboxOffset = { 0.6f, -0.45f, 0.f };
		jumpKick.hitboxHalfExtents = { 0.3f, 0.2f, 0.25f };
		jumpKick.damage = 2;
		jumpKick.knockback = 1.2f;
		jumpKick.airborne = true;
		combat.moves["melee.jumpKick"] = jumpKick;

		//--- uzis: hold the button and she raises the guns, holds the firing pose and keeps shooting; each shot is a bullet that
		//leaves from the barrel of one of the guns, which take turns ---
		AttackMove fire{};
		fire.animation = "uzi.fire";
		//the animation reaches its last frame, the firing pose, at 3/16 s and the guns start flashing a frame before; from
		//then the move waits there, shooting, for as long as the button is held. Letting go ends the move a moment later and
		//the guns are lowered (uzi.lower).
		fire.holdAt = 3.f / 16.f;
		fire.duration = fire.holdAt + 0.06f;
		fire.activeStart = 2.f / 16.f;
		fire.activeEnd = fire.holdAt; //so nothing fires after the button is let go
		fire.shotInterval = 0.08f; //the two guns take turns, so each fires about six times a second
		fire.repeats = true; //still holding after a hit knocked her out of it: raise the guns again
		fire.next = "uzi.lower";
		fire.muzzleOffset = { 0.55f, -0.64f, 0.f }; //only if a frame has no muzzle points
		fire.bulletSpeed = 11.f;
		fire.bulletLife = 0.55f;
		fire.damage = 1;
		fire.knockback = 0.3f;
		fire.hitStun = 0.15f;
		combat.moves["uzi.fire"] = fire;

		AttackMove jumpFire{};
		jumpFire.animation = "uzi.jumpFire";
		jumpFire.duration = 0.6f;
		jumpFire.activeStart = 0.1f;
		jumpFire.activeEnd = 0.45f;
		jumpFire.shotInterval = 0.07f;
		jumpFire.muzzleOffset = { 0.5f, -0.75f, 0.f };
		jumpFire.bulletSpeed = 11.f;
		jumpFire.bulletLife = 0.55f;
		jumpFire.damage = 1;
		jumpFire.knockback = 0.3f;
		jumpFire.hitStun = 0.15f;
		jumpFire.airborne = true;
		combat.moves["uzi.jumpFire"] = jumpFire;

		//--- katanas: long blades, so a lot of reach ---
		AttackMove slash{};
		slash.animation = "katana.attack";
		slash.duration = 5.f / 18.f;
		slash.activeStart = 1.f / 18.f; //the slash is sweeping on frames 2 to 4
		slash.activeEnd = 4.f / 18.f;
		slash.hitboxOffset = { 0.6f, -0.6f, 0.f };
		slash.hitboxHalfExtents = { 0.55f, 0.4f, 0.3f };
		slash.damage = 2;
		slash.knockback = 1.f;
		slash.hitStun = 0.35f;
		combat.moves["katana.attack"] = slash;

		AttackMove heavy{};
		heavy.animation = "katana.heavy";
		heavy.duration = 5.f / 12.f;
		heavy.activeStart = 1.f / 12.f; //spin, overhead and downward cut
		heavy.activeEnd = 4.f / 12.f;
		heavy.hitboxOffset = { 0.55f, -0.55f, 0.f };
		heavy.hitboxHalfExtents = { 0.8f, 0.55f, 0.35f };
		heavy.damage = 3;
		heavy.knockback = 2.f;
		heavy.hitStun = 0.5f;
		heavy.cooldown = 0.7f;
		heavy.cooldownGroup = "katana.heavy";
		combat.moves["katana.heavy"] = heavy;

		AttackMove airSlash{};
		airSlash.animation = "katana.airAttack";
		airSlash.duration = 0.6f;
		airSlash.activeStart = 0.25f; //the spinning slash is the 4th frame
		airSlash.activeEnd = 0.6f;
		airSlash.hitboxOffset = { 0.45f, -0.55f, 0.f };
		airSlash.hitboxHalfExtents = { 0.7f, 0.5f, 0.35f };
		airSlash.damage = 2;
		airSlash.knockback = 1.2f;
		airSlash.airborne = true;
		combat.moves["katana.airAttack"] = airSlash;

		//--- swapping weapons and picking things up: they lock her in place but hit nothing ---
		combat.moves["uzi.lower"] = idleMove("uzi.lower", 2.f, 16.f);
		combat.moves["equip.uzi"] = idleMove("uzi.equip", 8.f, 12.f);
		combat.moves["unequip.uzi"] = idleMove("uzi.unequip", 8.f, 14.f);
		combat.moves["equip.katana"] = idleMove("katana.equip", 8.f, 10.f);
		combat.moves["unequip.katana"] = idleMove("katana.unequip", 8.f, 12.f);
		combat.moves["melee.pickup"] = idleMove("melee.pickup", 8.f, 10.f);
		combat.moves["uzi.pickup"] = idleMove("uzi.pickup", 8.f, 10.f);
		combat.moves["katana.pickup"] = idleMove("katana.pickup", 8.f, 10.f);
		return combat;
	}

	const char* weaponName(WeaponMode mode)
	{
		switch (mode) {
		case WeaponMode::Uzi: return "uzi";
		case WeaponMode::Katana: return "katana";
		default: return "melee";
		}
	}

	void setWeaponMode(CharacterAnimations& animations, AttackBindings& bindings, Weapons& weapons, WeaponMode mode)
	{
		weapons.mode = mode;
		const std::string prefix = std::string(weaponName(mode)) + ".";
		animations.idle = prefix + "idle";
		animations.walk = prefix + "walk";
		animations.run = prefix + "run";
		animations.jump = prefix + "jump";
		animations.hurt = prefix + "hurt";
		switch (mode) {
		case WeaponMode::Melee:
			bindings = { "melee.punch", "melee.kick", "melee.jumpPunch", "melee.jumpKick", "", "" };
			break;
		case WeaponMode::Uzi: //both buttons fire
			bindings = { "uzi.fire", "uzi.fire", "uzi.jumpFire", "uzi.jumpFire", "", "" };
			break;
		case WeaponMode::Katana:
			bindings = { "katana.attack", "katana.heavy", "katana.airAttack", "katana.airAttack", "", "" };
			break;
		}
	}

	ecs::Entity spawnSummers(ecs::Registry& registry, const SummersAssets& assets, const glm::vec3& position)
	{
		//Her position is her feet, like the sprite's pivot, so her body box sits on top of it (-y is up): the same 1 unit
		//tall, half a unit wide and deep body Blaze had. Her sprite is a little taller than that, with the ponytail.
		const ecs::Entity summers = registry.create();
		registry.add<Transform>(summers, Transform{ position });
		registry.add<Sprite>(summers, assets.sheet, assets.texture, assets.pixelsPerUnit);
		registry.add<RigidBody>(summers);
		registry.add<Collider>(summers, Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }, false));
		registry.add<PlayerController>(summers);
		CharacterAnimations& animations = registry.add<CharacterAnimations>(summers, CharacterAnimations{});
		AttackBindings& bindings = registry.add<AttackBindings>(summers);
		Weapons& weapons = registry.add<Weapons>(summers);
		setWeaponMode(animations, bindings, weapons, WeaponMode::Melee);
		registry.add<Combat>(summers, summersCombat());
		registry.add<Health>(summers, Health{ 20, 20, 0.f, false }); //at 0 hp she stays down and the game over screen takes over (see gameFlow)
		registry.add<Name>(summers, Name{ "Summers (player)" });
		return summers;
	}
}
