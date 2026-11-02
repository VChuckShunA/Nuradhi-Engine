#include "game/characters/summers_sheets.hpp"

//std
#include <algorithm>
#include <cstdint>
#include <string>

namespace game {

	namespace {
		//Heights of a reference pose compared to a relaxed stand, for sheets that don't open on one
		constexpr float GUARD = 0.955f; //fighting stance, knees a little bent
		constexpr float RUNNING = 1.03f; //mid-stride: leaning, so a little shorter than standing, but drawn with a smaller head
		constexpr float CROUCH = 0.816f; //crouched to jump

		nrd::SpriteSource sheet(const char* name, const char* file, int frames, int referenceFrame = 0, float referenceHeight = 1.f)
		{
			nrd::SpriteSource source{};
			source.name = name;
			source.path = std::string("Images/Sprites/Summers/") + file;
			source.frames = frames;
			source.referenceFrame = referenceFrame;
			source.referenceHeight = referenceHeight;
			return source;
		}

		//katana sheets have glowing blades that reach down past the shoes
		nrd::SpriteSource katana(const char* name, const char* file, int frames, int referenceFrame = 0, float referenceHeight = 1.f)
		{
			nrd::SpriteSource source = sheet(name, file, frames, referenceFrame, referenceHeight);
			source.ignoreBlueForFeet = true;
			return source;
		}
	}

	std::vector<nrd::SpriteSource> summersSheets()
	{
		std::vector<nrd::SpriteSource> sheets;

		//melee
		sheets.push_back(sheet("melee.idle", "Melee/Summers idle.png", 8));
		sheets.push_back(sheet("melee.walk", "Melee/Summers Walk.png", 8));
		sheets.push_back(sheet("melee.run", "Melee/Summers Running.png", 8, 0, RUNNING));
		sheets.push_back(sheet("melee.jump", "Melee/Summers Jumping.png", 8));
		sheets.push_back(sheet("melee.punch", "Melee/Summers Punch.png", 8));
		sheets.push_back(sheet("melee.kick", "Melee/Summers Kick.png", 12, 0, GUARD));
		sheets.push_back(sheet("melee.jumpPunch", "Melee/Summers Jump Punch.png", 10, 0, GUARD));
		sheets.push_back(sheet("melee.jumpKick", "Melee/Summers Jump Kick.png", 10, 9, GUARD));
		sheets.push_back(sheet("melee.pickup", "Melee/Summers Pickup.png", 8));
		sheets.push_back(sheet("melee.hurt", "Melee/Summers Hit React.png", 8, 0, GUARD));

		//dual uzis
		sheets.push_back(sheet("uzi.idle", "Uzi/Summers Uzi Idle.png", 8));
		sheets.push_back(sheet("uzi.walk", "Uzi/Summers Uzi Walk.png", 8));
		sheets.push_back(sheet("uzi.run", "Uzi/Summers Uzi Run.png", 8, 0, RUNNING));
		sheets.push_back(sheet("uzi.jump", "Uzi/Summers Uzi jump.png", 8));
		sheets.push_back(sheet("uzi.jumpFire", "Uzi/Summers Jump fire.png", 9, 0, CROUCH));
		sheets.push_back(sheet("uzi.fire", "Uzi/Summers Uzi fire.png", 8));
		sheets.push_back(sheet("uzi.equip", "Uzi/Summers Uzi Equip.png", 8));
		sheets.push_back(sheet("uzi.pickup", "Uzi/Sumers Uzi pickup.png", 8));
		sheets.push_back(sheet("uzi.hurt", "Uzi/Summers Uzi hit react.png", 8));

		//dual katanas
		sheets.push_back(katana("katana.idle", "Katana/Summers Katana Idle.png", 8));
		sheets.push_back(katana("katana.walk", "Katana/Summers Katana Walk.png", 8));
		sheets.push_back(katana("katana.run", "Katana/Summers Katana Run.png", 8, 0, RUNNING));
		sheets.push_back(katana("katana.jump", "Katana/Summers Kata Jump.png", 8));
		sheets.push_back(katana("katana.airAttack", "Katana/Summers katana air attack.png", 11));
		sheets.push_back(katana("katana.attack", "Katana/Summers Katana Attack.png", 12));
		sheets.back().sharedGround = true; //a grounded combo whose slashes sweep down past the shoes
		sheets.push_back(katana("katana.equip", "Katana/Summers Katana Equip.png", 8));
		sheets.push_back(katana("katana.pickup", "Katana/Summers Katana Pickup.png", 8));
		sheets.push_back(katana("katana.hurt", "Katana/Summers Katana Hit react .png", 8));
		return sheets;
	}

	std::vector<glm::vec2> gunMuzzles(const nrd::SpriteAtlas& atlas, int frameIndex)
	{
		const nrd::SpriteFrame& frame = atlas.sheet->frames()[frameIndex];
		const int width = frame.size.x, height = frame.size.y;
		const std::uint8_t* pixels = atlas.pixels.data();
		const auto isGun = [&](int x, int y) {
			const std::uint8_t* p = pixels + (static_cast<size_t>(frame.position.y + y) * atlas.width + frame.position.x + x) * 4;
			if (p[3] < 200) return false;
			const int top = std::max({ p[0], p[1], p[2] }), bottom = std::min({ p[0], p[1], p[2] });
			return top < 75 && top - bottom < 45; //near-black and grey: not skin outlines, not the blue tie
		};

		//only the front of the frame, below the head: that's where a gun held out in front of her is
		const int fromX = static_cast<int>(width * 0.45f), fromY = static_cast<int>(height * 0.15f), toY = static_cast<int>(height * 0.75f);
		int frontmost = -1;
		for (int y = fromY; y < toY; y++) {
			for (int x = width - 1; x >= fromX; x--) {
				if (isGun(x, y)) {
					frontmost = std::max(frontmost, x);
					break;
				}
			}
		}
		if (frontmost < 0) return {};

		//the tips: gun pixels in the last few pixels before the front, grouped by height, so two guns give two points
		const int depth = std::max(6, static_cast<int>(width * 0.05f));
		std::vector<glm::ivec2> tip;
		for (int y = fromY; y < toY; y++) {
			for (int x = frontmost; x > frontmost - depth && x >= fromX; x--) {
				if (isGun(x, y)) tip.push_back({ x, y });
			}
		}
		std::sort(tip.begin(), tip.end(), [](const glm::ivec2& a, const glm::ivec2& b) { return a.y < b.y; });
		const int gap = std::max(3, static_cast<int>(height * 0.02f));
		std::vector<glm::vec2> muzzles;
		size_t start = 0;
		for (size_t i = 1; i <= tip.size(); i++) {
			if (i < tip.size() && tip[i].y - tip[i - 1].y <= gap) continue;
			if (i - start >= 4) { //a real gun, not a stray pixel
				float sumY = 0.f;
				int frontX = 0;
				for (size_t k = start; k < i; k++) {
					sumY += static_cast<float>(tip[k].y);
					frontX = std::max(frontX, tip[k].x);
				}
				muzzles.push_back({ static_cast<float>(frontX) + 0.5f, sumY / static_cast<float>(i - start) + 0.5f });
			}
			start = i;
		}
		return muzzles;
	}}
