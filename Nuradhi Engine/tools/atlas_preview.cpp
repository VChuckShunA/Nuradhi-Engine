//A small command line check of the sprite pipeline, without the renderer: how a sheet's background comes out, how many
//frames the analyser finds, and what the packed atlas looks like. Not part of the game's project.
//
//Build (from the project folder, in a Developer Command Prompt):
//  cl /nologo /std:c++latest /O2 /EHsc /I. /I"<stb folder>" /I"<glm folder>" tools\atlas_preview.cpp
//     engine\sprite\sprite.cpp engine\sprite\background_removal.cpp engine\sprite\sprite_atlas.cpp
//     game\characters\summers_sheets.cpp /Fe:atlas_preview.exe
//
//  atlas_preview count <sheet.png>...     frames found in each image, with no expected count
//  atlas_preview clean <in.png> <out.raw> the sheet with its background removed
//  atlas_preview atlas <out.raw>          Summers' atlas, and what the builder reports about each sheet
//  atlas_preview strip <out.raw> name:frame[*zoom]...  chosen frames side by side on one ground line, with a line at standing height
//  atlas_preview cache <folder>           bakes the atlas twice, compares the two, and checks that changes force a rebuild
//The .raw files are two 32-bit ints (width, height) and then RGBA pixels.

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "engine/sprite/background_removal.hpp"
#include "engine/sprite/sprite_atlas.hpp"
#include "game/characters/summers_sheets.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace nrd;

static void writeRaw(const char* path, const std::uint8_t* rgba, int width, int height)
{
	FILE* file = std::fopen(path, "wb");
	if (!file) { std::cerr << "can't write " << path << "\n"; return; }
	std::fwrite(&width, 4, 1, file);
	std::fwrite(&height, 4, 1, file);
	std::fwrite(rgba, 4, static_cast<size_t>(width) * height, file);
	std::fclose(file);
}

int main(int argc, char** argv)
{
	if (argc < 3) { std::cerr << "usage: see the comment at the top of tools/atlas_preview.cpp\n"; return 1; }
	const std::string mode = argv[1];
	const auto start = std::chrono::steady_clock::now();

	if (mode == "count") {
		for (int i = 2; i < argc; i++) {
			int w = 0, h = 0, c = 0;
			stbi_uc* data = stbi_load(argv[i], &w, &h, &c, STBI_rgb_alpha);
			if (!data) { std::cerr << "can't load " << argv[i] << "\n"; continue; }
			removeFlatBackground(data, w, h);
			SpriteSheetSettings settings{};
			settings.minPixels = 1500;
			settings.mergeDistance = 3;
			const SpriteSheet sheet = SpriteSheet::fromPixels(data, w, h, settings);
			std::cout << argv[i] << "  " << w << "x" << h << "  big pieces: " << sheet.frames().size() << " in " << sheet.rows().size() << " rows\n";
			for (size_t r = 0; r < sheet.rows().size(); r++) {
				std::cout << "  row " << r << ":";
				for (const int index : sheet.rows()[r]) {
					const SpriteFrame& f = sheet.frames()[index];
					std::cout << " [" << f.position.x << "," << f.position.y << " " << f.size.x << "x" << f.size.y << " feet " << static_cast<int>(f.pivot.y) << "]";
				}
				std::cout << "\n";
			}
			stbi_image_free(data);
		}
	}
	else if (mode == "clean" && argc >= 4) {
		int w = 0, h = 0, c = 0;
		stbi_uc* data = stbi_load(argv[2], &w, &h, &c, STBI_rgb_alpha);
		if (!data) { std::cerr << "can't load " << argv[2] << "\n"; return 1; }
		removeFlatBackground(data, w, h);
		writeRaw(argv[3], data, w, h);
		stbi_image_free(data);
	}
	else if (mode == "strip" && argc >= 4) {
		SpriteAtlasSettings settings{};
		const SpriteAtlas atlas = buildSpriteAtlas(game::summersSheets(), game::SUMMERS_STANDING_HEIGHT, settings);
		const int slot = 330, margin = 40, groundY = 640;
		const int count = argc - 3;
		const int width = slot * count, height = groundY + margin;
		std::vector<std::uint8_t> out(static_cast<size_t>(width) * height * 4, 0);
		const auto put = [&](int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
			if (x < 0 || y < 0 || x >= width || y >= height) return;
			std::uint8_t* p = out.data() + (static_cast<size_t>(y) * width + x) * 4;
			p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
		};
		for (int x = 0; x < width; x++) {
			put(x, groundY, 255, 255, 0); //the ground
			put(x, groundY - static_cast<int>(settings.standingHeight), 0, 255, 255); //standing height
		}
		for (int i = 0; i < count; i++) {
			const std::string arg = argv[3 + i];
			const size_t colon = arg.rfind(':');
			const std::string name = arg.substr(0, colon);
			const int column = std::atoi(arg.c_str() + colon + 1);
			const size_t star = arg.find('*');
			const float zoom = star == std::string::npos ? 1.f : static_cast<float>(std::atof(arg.c_str() + star + 1));
			const SpriteFrame& frame = atlas.sheet->frames()[atlas.frame(name, column)];
			const int originX = slot * i + slot / 2 - static_cast<int>(frame.pivot.x * zoom);
			const int originY = groundY - static_cast<int>(frame.pivot.y * zoom);
			for (int y = 0; y < static_cast<int>(frame.size.y * zoom); y++) {
				for (int x = 0; x < static_cast<int>(frame.size.x * zoom); x++) {
					const int sx = std::min(frame.size.x - 1, static_cast<int>(x / zoom)), sy = std::min(frame.size.y - 1, static_cast<int>(y / zoom));
					const std::uint8_t* src = atlas.pixels.data() + (static_cast<size_t>(frame.position.y + sy) * atlas.width + frame.position.x + sx) * 4;
					if (src[3] < 128) continue;
					put(originX + x, originY + y, src[0], src[1], src[2]);
				}
			}
			for (const glm::vec2& muzzle : game::gunMuzzles(atlas, atlas.frame(name, column))) { //red crosses where her guns' barrels are
				const int mx = originX + static_cast<int>(muzzle.x * zoom), my = originY + static_cast<int>(muzzle.y * zoom);
				for (int d = -7; d <= 7; d++) { put(mx + d, my, 255, 0, 0); put(mx, my + d, 255, 0, 0); }
			}
			put(slot * i, 0, 255, 0, 255);
			for (int y = 0; y < height; y += 6) put(slot * i, y, 255, 0, 255); //slot dividers
		}
		writeRaw(argv[2], out.data(), width, height);
	}	else if (mode == "cache") {
		namespace fs = std::filesystem;
		const fs::path folder = argv[2];
		fs::create_directories(folder);
		const std::string bakePath = (folder / "summers.atlas").string();
		fs::remove(bakePath);
		SpriteAtlasSettings settings{};
		std::vector<SpriteSource> sources = game::summersSheets();
		int failures = 0;
		const auto check = [&](bool ok, const char* what) { std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n"; if (!ok) failures++; };
		const auto timed = [&](auto&& call) { const auto t = std::chrono::steady_clock::now(); auto result = call(); return std::make_pair(std::move(result), std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count()); };

		auto [first, firstSeconds] = timed([&] { return loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath); });
		auto [second, secondSeconds] = timed([&] { return loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath); });
		std::cout << "first call " << firstSeconds << " s (from cache: " << first.fromCache << "), second call " << secondSeconds << " s (from cache: " << second.fromCache << ")\n";
		std::cout << "baked file: " << fs::file_size(bakePath) / 1024 / 1024.0 << " MB for a " << first.width << "x" << first.height << " atlas (" << first.pixels.size() / 1024 / 1024 << " MB raw)\n";
		check(!first.fromCache, "first call builds");
		check(second.fromCache, "second call reads the bake");
		check(first.width == second.width && first.height == second.height, "same size");
		check(first.pixels == second.pixels, "every pixel identical");
		check(first.pixelsPerUnit == second.pixelsPerUnit, "same pixels per unit");
		check(first.rowOf == second.rowOf, "same sheet names and rows");
		bool framesMatch = first.sheet->frames().size() == second.sheet->frames().size();
		bool masksMatch = true;
		for (size_t i = 0; framesMatch && i < first.sheet->frames().size(); i++) {
			const SpriteFrame& a = first.sheet->frames()[i];
			const SpriteFrame& b = second.sheet->frames()[i];
			framesMatch = a.row == b.row && a.position == b.position && a.size == b.size && a.pivot == b.pivot && a.pixelCount == b.pixelCount && a.uvRect == b.uvRect;
		}
		for (int y = 0; masksMatch && y < first.height; y += 3) for (int x = 0; masksMatch && x < first.width; x += 3) masksMatch = first.sheet->isOpaque(x, y) == second.sheet->isOpaque(x, y);
		check(framesMatch, "every frame's position, size, pivot and uv identical");
		check(masksMatch, "opaque mask identical");
		check(first.sheet->rows() == second.sheet->rows(), "same rows and columns");

		//changes that must force a rebuild
		sources[3].scale = 1.01f;
		check(!loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "changing a sheet's scale rebuilds");
		check(loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "...and the new bake is then reused");
		settings.standingHeight = 500.f;
		check(!loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "changing the atlas settings rebuilds");
		settings = SpriteAtlasSettings{};
		sources = game::summersSheets();
		const fs::path copy = folder / "copy.png"; //a source image of our own to touch, so the real assets stay untouched
		fs::copy_file(sources[0].path, copy, fs::copy_options::overwrite_existing);
		sources[0].path = copy.string();
		loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath);
		check(loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "an untouched source reuses the bake");
		fs::last_write_time(copy, fs::file_time_type::clock::now() + std::chrono::hours(1));
		check(!loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "touching a source image rebuilds");

		//damage must never be loaded
		loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath);
		{
			std::ifstream in(bakePath, std::ios::binary);
			std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			in.close();
			bytes.resize(bytes.size() - 5000); //cut off the end, as a crash while writing without the safe rename would
			std::ofstream out(bakePath, std::ios::binary | std::ios::trunc);
			out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		check(!loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "a truncated file is rebuilt, not loaded");
		check(loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "...and replaced with a good one");
		{
			std::ofstream out(bakePath, std::ios::binary | std::ios::trunc);
			out << "this is not an atlas\n";
		}
		check(!loadOrBuildSpriteAtlas(sources, game::SUMMERS_STANDING_HEIGHT, settings, bakePath).fromCache, "a file of nonsense is rebuilt, not loaded");
		std::cout << (failures == 0 ? "all checks passed\n" : "SOME CHECKS FAILED\n");
		fs::remove(copy);
		return failures == 0 ? 0 : 2;
	}	else if (mode == "atlas") {
		SpriteAtlasSettings settings{};
		settings.describe = true;
		const SpriteAtlas atlas = buildSpriteAtlas(game::summersSheets(), game::SUMMERS_STANDING_HEIGHT, settings);
		std::cout << "atlas " << atlas.width << "x" << atlas.height << ", " << atlas.sheet->frames().size() << " frames, "
			<< atlas.pixelsPerUnit << " px per unit\n";
		writeRaw(argv[2], atlas.pixels.data(), atlas.width, atlas.height);
	}
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::cout << "took " << seconds << " s\n";
	return 0;
}
