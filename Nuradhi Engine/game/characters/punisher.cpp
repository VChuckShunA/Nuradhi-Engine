#include "game/characters/punisher.hpp"
#include "game/characters/summers_sheets.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/collision.hpp"
#include "engine/physics/physics.hpp"
#include "engine/render/renderer.hpp"
#include "engine/sprite/sprite_atlas.hpp"

//std
#include <chrono>
#include <filesystem>
#include <iostream>

namespace game {

	using namespace nrd;

	namespace {
		constexpr const char* SHEET = "Images/Sprites/Arcade - The Punisher - Playable Characters - Punisher _ Frank Castle.png";
		constexpr const char* BAKE = "Images/Sprites/Punisher/baked/punisher.atlas";

		//A relaxed standing frame is 84 pixels tall on the sheet; he is 1.1 world units tall. The frames keep every pixel as
		//drawn (scale 1) and are shown with the pixelated filter, so the arcade art stays crisp.
		constexpr float STANDING_HEIGHT_PIXELS = 84.f;
		constexpr float STANDING_HEIGHT_UNITS = 1.1f;

		//One row of the sheet: the rectangle that holds its frames (x, y, width, height in sheet pixels) and how many there are.
		//The sheet is drawn on flat cyan, which becomes transparent.
		SpriteSource row(const char* name, int x, int y, int width, int height, int frames)
		{
			SpriteSource source{};
			source.name = name;
			source.path = SHEET;
			source.frames = frames;
			source.cropX = x;
			source.cropY = y;
			source.cropWidth = width;
			source.cropHeight = height;
			source.useKey = true;
			source.key[0] = 0;
			source.key[1] = 248;
			source.key[2] = 248;
			source.removeBackground = false;
			source.pixelScale = 1.f;
			source.mergeDistance = 4;
			return source;
		}

		//The rows, found by looking at the sheet. Frame numbers below count from the left of each row.
		std::vector<SpriteSource> punisherSheets()
		{
			std::vector<SpriteSource> sheets;
			sheets.push_back(row("idle", 20, 20, 260, 100, 3)); //a breathing guard
			sheets.push_back(row("walk", 10, 128, 1000, 100, 12));
			sheets.push_back(row("jump", 300, 15, 390, 105, 4)); //standing, tucked, the kick starting, the flying kick
			sheets.push_back(row("punch", 15, 245, 490, 105, 5)); //guard, two jabs, back to guard
			sheets.push_back(row("kick", 195, 360, 385, 110, 4)); //crouch, the leg swinging up, the high kick, recover
			sheets.push_back(row("hurt", 15, 3045, 90, 100, 1)); //flinching from a blow: the first frame of the third row from the bottom
			sheets.push_back(row("gunDraw", 20, 1553, 680, 105, 8)); //the pistol coming up to aim (0 to 4), then running and jumping with it (5 to 7)
			sheets.push_back(row("gunFire", 15, 1775, 790, 105, 6)); //firing: muzzle flashes on 0 to 2, then the smoke
			return sheets;
		}

		void animate(SpriteSheet& sheet, const SpriteAtlas& atlas, const std::string& name, const std::vector<std::pair<const char*, int>>& frames,
			float framesPerSecond, bool loop, int loopStart = 0)
		{
			SpriteAnimation animation{ name, {}, framesPerSecond, loop, loopStart };
			for (const auto& [source, column] : frames) animation.frames.push_back(atlas.frame(source, column));
			sheet.addAnimation(animation);
		}

		//every frame of one row, in order
		std::vector<std::pair<const char*, int>> whole(const SpriteAtlas& atlas, const char* source)
		{
			std::vector<std::pair<const char*, int>> frames;
			for (int i = 0; i < atlas.frameCount(source); i++) frames.push_back({ source, i });
			return frames;
		}

		SpriteAtlas makePunisherAtlas(bool describe, bool rebuild)
		{
			SpriteAtlasSettings settings{};
			settings.describe = describe;
			settings.standingHeight = STANDING_HEIGHT_PIXELS; //so a relaxed standing frame is STANDING_HEIGHT_UNITS tall
			settings.padding = 2;
			if (rebuild) {
				std::error_code error;
				std::filesystem::remove(BAKE, error);
			}
			SpriteAtlas atlas = loadOrBuildSpriteAtlas(punisherSheets(), STANDING_HEIGHT_UNITS, settings, BAKE);
			if (describe) std::cout << "Punisher's atlas is " << atlas.width << "x" << atlas.height << " with " << atlas.sheet->frames().size() << " frames\n";
			SpriteSheet& sheet = *atlas.sheet;

			animate(sheet, atlas, "idle", whole(atlas, "idle"), 5.f, true);
			animate(sheet, atlas, "walk", whole(atlas, "walk"), 12.f, true);
			animate(sheet, atlas, "run", whole(atlas, "walk"), 18.f, true); //the same stride, quicker
			animate(sheet, atlas, "jump", { { "jump", 1 }, { "jump", 2 } }, 6.f, false); //tucks as he leaves the ground, then holds
			animate(sheet, atlas, "hurt", whole(atlas, "hurt"), 8.f, false); //a single flinch, held for as long as he is stunned (also where he stays when beaten)

			animate(sheet, atlas, "punch", whole(atlas, "punch"), 20.f, false); //jabs on 1 and 2
			animate(sheet, atlas, "kick", whole(atlas, "kick"), 14.f, false); //the high kick is out on 2
			animate(sheet, atlas, "jumpPunch", { { "jump", 2 }, { "jump", 2 } }, 8.f, false);
			animate(sheet, atlas, "jumpKick", { { "jump", 2 }, { "jump", 3 } }, 8.f, false); //holds the flying kick

			//the pistol: it comes up in three frames, then the flashing frames repeat for as long as he holds the button
			animate(sheet, atlas, "gun", { { "gunDraw", 2 }, { "gunDraw", 3 }, { "gunDraw", 4 }, { "gunFire", 1 }, { "gunFire", 0 }, { "gunFire", 2 }, { "gunFire", 0 } }, 14.f, true, 3);
			animate(sheet, atlas, "jumpGun", { { "gunDraw", 6 }, { "gunDraw", 7 } }, 10.f, false);

			//where the barrel is on every frame he fires in, so bullets leave from the gun as drawn (see attackSystem)
			const auto anchor = [&](const char* source, int column) {
				const int frame = atlas.frame(source, column);
				sheet.setAnchors(frame, "muzzle", gunMuzzles(atlas, frame));
			};
			for (int column = 0; column < 3; column++) anchor("gunFire", column);
			anchor("gunDraw", 4);
			anchor("gunDraw", 6);
			anchor("gunDraw", 7);
			return atlas;
		}
	}

	PunisherAssets loadPunisher(Renderer& renderer, bool describe)
	{
		const auto started = std::chrono::steady_clock::now();
		SpriteAtlas atlas = makePunisherAtlas(describe, false);
		const auto atlasBuilt = std::chrono::steady_clock::now();
		PunisherAssets assets;
		assets.texture = renderer.loadTextureFromPixels(atlas.pixels.data(), atlas.width, atlas.height, TextureFilter::Pixelated);
		assets.sheet = atlas.sheet;
		assets.pixelsPerUnit = atlas.pixelsPerUnit;
		std::cout << "Punisher: atlas " << (atlas.fromCache ? "loaded from the baked file in " : "built and baked in ") << std::chrono::duration<double>(atlasBuilt - started).count() << " s" << std::endl;
		return assets;
	}

	std::string rebakePunisher(Renderer& renderer, ecs::Registry& registry, PunisherAssets& assets)
	{
		const auto started = std::chrono::steady_clock::now();
		//every animator points into the sheet's animations, which are about to be replaced: note what each sprite plays
		std::vector<std::pair<ecs::Entity, std::string>> playing;
		registry.each<Sprite>([&](ecs::Entity entity, Sprite& sprite) {
			if (&sprite.animator.sheet() == assets.sheet.get()) playing.push_back({ entity, sprite.animator.currentAnimation() });
		});

		SpriteAtlas atlas = makePunisherAtlas(false, true);
		renderer.replaceTextureFromPixels(assets.texture, atlas.pixels.data(), atlas.width, atlas.height, TextureFilter::Pixelated);
		*assets.sheet = std::move(*atlas.sheet); //the same object, so every sprite sees the new frames
		assets.pixelsPerUnit = atlas.pixelsPerUnit;
		for (const auto& [entity, animation] : playing) {
			Sprite& sprite = registry.get<Sprite>(entity);
			sprite.pixelsPerUnit = assets.pixelsPerUnit;
			if (!animation.empty()) sprite.animator.play(animation, true);
		}
		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
		return "Rebaked the Punisher's sprites in " + std::to_string(seconds).substr(0, 4) + " s (" + std::to_string(assets.sheet->frames().size()) + " frames).";
	}

	Combat punisherCombat()
	{
		//World units from his feet as if he faces right; -y is up. He is about 1.1 tall, his fists at chest height around y = -0.75.
		Combat combat{};

		AttackMove punch{};
		punch.animation = "punch";
		punch.duration = 5.f / 20.f;
		punch.activeStart = 1.f / 20.f; //the jabs are out on the 2nd and 3rd frames
		punch.activeEnd = 3.f / 20.f;
		punch.hitboxOffset = { 0.48f, -0.75f, 0.f };
		punch.hitboxHalfExtents = { 0.2f, 0.12f, 0.25f };
		punch.damage = 1;
		punch.knockback = 0.5f;
		combat.moves["punch"] = punch;

		AttackMove kick{};
		kick.animation = "kick";
		kick.duration = 4.f / 14.f;
		kick.activeStart = 1.5f / 14.f; //the leg is out on the 3rd frame
		kick.activeEnd = 3.f / 14.f;
		kick.hitboxOffset = { 0.6f, -0.75f, 0.f };
		kick.hitboxHalfExtents = { 0.3f, 0.2f, 0.25f };
		kick.damage = 2;
		kick.knockback = 1.f;
		combat.moves["kick"] = kick;

		//jump attacks hold their pose and stay live until he lands (a jump lasts about 0.7 s)
		AttackMove jumpPunch{};
		jumpPunch.animation = "jumpPunch";
		jumpPunch.duration = 0.6f;
		jumpPunch.activeStart = 0.1f;
		jumpPunch.activeEnd = 0.6f;
		jumpPunch.hitboxOffset = { 0.45f, -0.6f, 0.f };
		jumpPunch.hitboxHalfExtents = { 0.25f, 0.18f, 0.25f };
		jumpPunch.damage = 1;
		jumpPunch.knockback = 0.8f;
		jumpPunch.airborne = true;
		combat.moves["jumpPunch"] = jumpPunch;

		AttackMove jumpKick{};
		jumpKick.animation = "jumpKick";
		jumpKick.duration = 0.6f;
		jumpKick.activeStart = 0.1f;
		jumpKick.activeEnd = 0.6f;
		jumpKick.hitboxOffset = { 0.6f, -0.5f, 0.f };
		jumpKick.hitboxHalfExtents = { 0.32f, 0.2f, 0.25f };
		jumpKick.damage = 2;
		jumpKick.knockback = 1.2f;
		jumpKick.airborne = true;
		combat.moves["jumpKick"] = jumpKick;

		//the pistol: the gun comes up (3 frames at 14 per second), then he keeps firing while the button is held; each shot is a
		//bullet from the barrel. Letting go ends the move a moment later.
		AttackMove fire{};
		fire.animation = "gun";
		fire.holdAt = 3.f / 14.f;
		fire.duration = fire.holdAt + 0.12f;
		fire.activeStart = fire.holdAt - 0.02f;
		fire.activeEnd = fire.holdAt; //so nothing fires after the button is let go
		fire.shotInterval = 0.2f;
		fire.repeats = true; //still holding after a hit knocked him out of it: draw again
		fire.muzzleOffset = { 0.55f, -0.72f, 0.f }; //only if a frame has no muzzle point
		fire.bulletSpeed = 12.f;
		fire.bulletLife = 0.55f;
		fire.damage = 2;
		fire.knockback = 0.4f;
		fire.hitStun = 0.2f;
		combat.moves["special"] = fire;

		AttackMove jumpFire{};
		jumpFire.animation = "jumpGun";
		jumpFire.duration = 0.6f;
		jumpFire.activeStart = 0.05f;
		jumpFire.activeEnd = 0.45f;
		jumpFire.shotInterval = 0.2f;
		jumpFire.muzzleOffset = { 0.5f, -0.75f, 0.f };
		jumpFire.bulletSpeed = 12.f;
		jumpFire.bulletLife = 0.55f;
		jumpFire.damage = 2;
		jumpFire.knockback = 0.4f;
		jumpFire.hitStun = 0.2f;
		jumpFire.airborne = true;
		combat.moves["jumpSpecial"] = jumpFire;
		return combat;
	}

	ecs::Entity spawnPunisher(ecs::Registry& registry, const PunisherAssets& assets, const glm::vec3& position)
	{
		//His position is his feet, like the sprite's pivot, so his body box sits on top of it (-y is up): the same 1 unit tall,
		//half a unit wide and deep body Summers has.
		const ecs::Entity punisher = registry.create();
		registry.add<Transform>(punisher, Transform{ position });
		registry.add<Sprite>(punisher, assets.sheet, assets.texture, assets.pixelsPerUnit);
		registry.add<RigidBody>(punisher);
		registry.add<Collider>(punisher, Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }, false));
		registry.add<PlayerController>(punisher);
		CharacterAnimations& animations = registry.add<CharacterAnimations>(punisher, CharacterAnimations{});
		animations.idle = "idle";
		animations.walk = "walk";
		animations.run = "run";
		animations.jump = "jump";
		animations.hurt = "hurt";
		registry.add<Combat>(punisher, punisherCombat());
		registry.add<Health>(punisher, Health{ 20, 20, 0.f, false }); //at 0 hp he stays down and the game over screen takes over (see gameFlow)
		registry.add<Name>(punisher, Name{ "Punisher (player)" });
		return punisher;
	}
}
