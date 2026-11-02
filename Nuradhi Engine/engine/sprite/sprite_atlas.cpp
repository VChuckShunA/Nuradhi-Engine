#include "engine/sprite/sprite_atlas.hpp"
#include "engine/sprite/background_removal.hpp"

//libs
#include <stb_image.h>

//std
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace nrd {

	namespace {
		//a frame cut out of its sheet, resampled, waiting to be packed
		struct Piece {
			int source = 0;
			SpriteFrame frame; //size, pivot and pixel count in atlas pixels; position still to be decided
			std::vector<std::uint8_t> rgba; //size.x * size.y * 4
		};

		//Cuts `frame` out of `image` (only the pixels that belong to it) and scales it by `scale`. Averaging is done on
		//alpha-weighted colour, so a transparent pixel's colour never leaks into the sprite. Scaling down boxes
		//the source pixels; scaling up samples bilinearly.
		Piece cutFrame(const std::vector<std::uint8_t>& image, int imageWidth, const SpriteSheet& sheet, int frameIndex, float scale)
		{
			const SpriteFrame& source = sheet.frames()[frameIndex];
			Piece piece{};
			piece.frame.size = { std::max(1, static_cast<int>(std::ceil(source.size.x * scale))), std::max(1, static_cast<int>(std::ceil(source.size.y * scale))) };
			piece.frame.pivot = source.pivot * scale;
			piece.frame.pixelCount = static_cast<int>(source.pixelCount * scale * scale);
			piece.rgba.assign(static_cast<size_t>(piece.frame.size.x) * piece.frame.size.y * 4, 0);

			//the frame's own pixels, premultiplied, in a buffer of the frame's size
			const int sw = source.size.x, sh = source.size.y;
			std::vector<float> premultiplied(static_cast<size_t>(sw) * sh * 4, 0.f);
			for (int y = 0; y < sh; y++) {
				for (int x = 0; x < sw; x++) {
					const int ix = source.position.x + x, iy = source.position.y + y;
					if (sheet.frameIdAt(ix, iy) != frameIndex) continue;
					const std::uint8_t* p = image.data() + (static_cast<size_t>(iy) * imageWidth + ix) * 4;
					const float a = p[3] / 255.f;
					float* out = premultiplied.data() + (static_cast<size_t>(y) * sw + x) * 4;
					out[0] = p[0] * a; out[1] = p[1] * a; out[2] = p[2] * a; out[3] = a;
				}
			}
			const auto at = [&](int x, int y) { return premultiplied.data() + (static_cast<size_t>(std::clamp(y, 0, sh - 1)) * sw + std::clamp(x, 0, sw - 1)) * 4; };

			for (int oy = 0; oy < piece.frame.size.y; oy++) {
				for (int ox = 0; ox < piece.frame.size.x; ox++) {
					float acc[4] = { 0.f, 0.f, 0.f, 0.f };
					if (scale <= 1.f) {
						const float x0 = ox / scale, x1 = std::min((ox + 1) / scale, static_cast<float>(sw));
						const float y0 = oy / scale, y1 = std::min((oy + 1) / scale, static_cast<float>(sh));
						float weight = 0.f;
						for (int iy = static_cast<int>(y0); iy < static_cast<int>(std::ceil(y1)) && iy < sh; iy++) {
							const float wy = std::min(y1, iy + 1.f) - std::max(y0, static_cast<float>(iy));
							for (int ix = static_cast<int>(x0); ix < static_cast<int>(std::ceil(x1)) && ix < sw; ix++) {
								const float w = wy * (std::min(x1, ix + 1.f) - std::max(x0, static_cast<float>(ix)));
								const float* p = at(ix, iy);
								for (int k = 0; k < 4; k++) acc[k] += p[k] * w;
								weight += w;
							}
						}
						if (weight > 0.f) for (int k = 0; k < 4; k++) acc[k] /= weight;
					}
					else {
						const float sx = (ox + 0.5f) / scale - 0.5f, sy = (oy + 0.5f) / scale - 0.5f;
						const int ix = static_cast<int>(std::floor(sx)), iy = static_cast<int>(std::floor(sy));
						const float fx = sx - ix, fy = sy - iy;
						for (int k = 0; k < 4; k++) {
							acc[k] = (at(ix, iy)[k] * (1.f - fx) + at(ix + 1, iy)[k] * fx) * (1.f - fy) +
								(at(ix, iy + 1)[k] * (1.f - fx) + at(ix + 1, iy + 1)[k] * fx) * fy;
						}
					}
					std::uint8_t* out = piece.rgba.data() + (static_cast<size_t>(oy) * piece.frame.size.x + ox) * 4;
					if (acc[3] > 1e-4f) {
						for (int k = 0; k < 3; k++) out[k] = static_cast<std::uint8_t>(std::clamp(acc[k] / acc[3] + 0.5f, 0.f, 255.f));
						out[3] = static_cast<std::uint8_t>(std::clamp(acc[3] * 255.f + 0.5f, 0.f, 255.f));
					}
				}
			}
			return piece;
		}
	}

	int SpriteAtlas::row(const std::string& name) const
	{
		const auto found = rowOf.find(name);
		if (found == rowOf.end()) throw std::invalid_argument("Sprite atlas has no source called " + name);
		return found->second;
	}

	SpriteAtlas buildSpriteAtlas(const std::vector<SpriteSource>& sources, float unitsPerStandingHeight, const SpriteAtlasSettings& settings)
	{
		SpriteAtlas atlas{};
		using Clock = std::chrono::steady_clock;
		const Clock::time_point started = Clock::now();
		for (int sourceIndex = 0; sourceIndex < static_cast<int>(sources.size()); sourceIndex++) atlas.rowOf[sources[sourceIndex].name] = sourceIndex;

		//Every sheet is independent, so they're loaded, cleaned, analysed and cut on several threads at once
		std::vector<std::vector<Piece>> piecesOf(sources.size());
		std::vector<std::string> reports(sources.size());
		std::vector<std::string> failures(sources.size());
		std::atomic<int> nextSource{ 0 };
		std::atomic<long long> loadMicros{ 0 }, cleanMicros{ 0 }, analyseMicros{ 0 }, cutMicros{ 0 };
		const auto micros = [](Clock::time_point from) { return static_cast<long long>(std::chrono::duration<double, std::micro>(Clock::now() - from).count()); };
		const auto work = [&]() {
			for (int sourceIndex = nextSource++; sourceIndex < static_cast<int>(sources.size()); sourceIndex = nextSource++) {
				const SpriteSource& source = sources[sourceIndex];
				try {
					Clock::time_point mark = Clock::now();
					int width = 0, height = 0, channels = 0;
					std::unique_ptr<stbi_uc, void (*)(void*)> loaded{ stbi_load(source.path.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free };
					if (!loaded) throw std::runtime_error("Failed to load sprite sheet " + source.path + ": " + stbi_failure_reason());
					std::vector<std::uint8_t> image(loaded.get(), loaded.get() + static_cast<size_t>(width) * height * 4);
					loaded.reset();
					loadMicros += micros(mark); mark = Clock::now();
					if (source.removeBackground) removeFlatBackground(image.data(), width, height);
					cleanMicros += micros(mark); mark = Clock::now();

					SpriteSheetSettings analysis{};
					analysis.expectedFrames = source.frames;
					analysis.minPixels = 4;
					analysis.mergeDistance = source.mergeDistance;
					analysis.ignoreBlueForFeet = source.ignoreBlueForFeet;
					analysis.sharedGround = source.sharedGround;
					analysis.keepFrameMap = true;
					const SpriteSheet sheet = SpriteSheet::fromPixels(image.data(), width, height, analysis);
					analyseMicros += micros(mark); mark = Clock::now();
					const int frameCount = static_cast<int>(sheet.frames().size());
					if (frameCount == 0) throw std::runtime_error("Found no sprites in " + source.path);
					if (source.referenceFrame < 0 || source.referenceFrame >= frameCount) {
						throw std::runtime_error(source.name + ": reference frame " + std::to_string(source.referenceFrame) + " isn't one of its " + std::to_string(frameCount) + " frames");
					}

					//scale: the reference frame's height from the top of its sprite to its feet becomes referenceHeight standing heights
					const float referencePixels = std::max(1.f, sheet.frames()[source.referenceFrame].pivot.y);
					const float scale = settings.standingHeight * source.referenceHeight / referencePixels * source.scale;
					std::ostringstream report;
					report << std::fixed << std::setprecision(3) << source.name << ": " << frameCount << " frames in " << sheet.rows().size()
						<< " rows, reference frame " << source.referenceFrame << " is " << referencePixels << " px tall, scale " << scale;
					reports[sourceIndex] = report.str();
					for (int i = 0; i < frameCount; i++) {
						Piece piece = cutFrame(image, width, sheet, i, scale);
						piece.source = sourceIndex;
						piece.frame.row = sourceIndex;
						piecesOf[sourceIndex].push_back(std::move(piece));
					}
					cutMicros += micros(mark);
				}
				catch (const std::exception& error) {
					failures[sourceIndex] = error.what();
				}
			}
		};
		//Four workers: measured, more than that is slower in a Debug build (the sheets are memory hungry and the Debug
		//runtime serialises more than it should), and in a Release build four is already well under a second
		const int workers = std::max(1, std::min({ static_cast<int>(std::thread::hardware_concurrency()), 4, static_cast<int>(sources.size()) }));
		std::vector<std::thread> threads;
		for (int i = 1; i < workers; i++) threads.emplace_back(work);
		work();
		for (std::thread& thread : threads) thread.join();
		for (const std::string& failure : failures) {
			if (!failure.empty()) throw std::runtime_error(failure);
		}
		if (settings.describe) for (const std::string& report : reports) std::cout << report << "\n";
		const Clock::time_point packStarted = Clock::now();

		std::vector<Piece> pieces;
		for (std::vector<Piece>& list : piecesOf) {
			for (Piece& piece : list) pieces.push_back(std::move(piece));
		}
		//shelf packing: tallest first, left to right, a new shelf when the row is full
		std::vector<int> order(pieces.size());
		for (int i = 0; i < static_cast<int>(order.size()); i++) order[i] = i;
		std::sort(order.begin(), order.end(), [&](int a, int b) { return pieces[a].frame.size.y > pieces[b].frame.size.y; });
		const int pad = settings.padding;
		int cursorX = pad, shelfY = pad, shelfHeight = 0, usedWidth = 0;
		for (const int index : order) {
			Piece& piece = pieces[index];
			if (cursorX + piece.frame.size.x + pad > settings.maxWidth) {
				cursorX = pad;
				shelfY += shelfHeight + pad;
				shelfHeight = 0;
			}
			piece.frame.position = { cursorX, shelfY };
			cursorX += piece.frame.size.x + pad;
			shelfHeight = std::max(shelfHeight, piece.frame.size.y);
			usedWidth = std::max(usedWidth, cursorX);
		}
		atlas.width = std::max(1, usedWidth);
		atlas.height = std::max(1, shelfY + shelfHeight + pad);

		atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4, 0);
		std::vector<std::uint8_t> mask(static_cast<size_t>(atlas.width) * atlas.height, 0);
		std::vector<SpriteFrame> frames;
		for (Piece& piece : pieces) {
			SpriteFrame& frame = piece.frame;
			for (int y = 0; y < frame.size.y; y++) {
				std::copy_n(piece.rgba.data() + static_cast<size_t>(y) * frame.size.x * 4, static_cast<size_t>(frame.size.x) * 4,
					atlas.pixels.data() + (static_cast<size_t>(frame.position.y + y) * atlas.width + frame.position.x) * 4);
			}
			frame.uvRect = {
				static_cast<float>(frame.position.x) / atlas.width, static_cast<float>(frame.position.y) / atlas.height,
				static_cast<float>(frame.size.x) / atlas.width, static_cast<float>(frame.size.y) / atlas.height };
			frames.push_back(frame);
		}
		{
			//raw pointers: this runs over every pixel of the atlas, and bounds-checked access makes Debug builds crawl
			const std::uint8_t* pixels = atlas.pixels.data();
			std::uint8_t* opaque = mask.data();
			for (size_t i = 0; i < mask.size(); i++) opaque[i] = pixels[i * 4 + 3] >= 128 ? 1 : 0;
		}
		bleedColour(atlas.pixels.data(), atlas.width, atlas.height, settings.padding);

		atlas.sheet = std::make_shared<SpriteSheet>(SpriteSheet::fromFrames({ atlas.width, atlas.height }, std::move(frames), std::move(mask)));
		atlas.pixelsPerUnit = settings.standingHeight / unitsPerStandingHeight;
		if (settings.describe) {
			std::cout << "atlas built in " << std::chrono::duration<double>(Clock::now() - started).count() << " s on " << workers << " threads (cpu seconds: load " << loadMicros / 1e6
				<< ", background " << cleanMicros / 1e6 << ", analyse " << analyseMicros / 1e6 << ", cut " << cutMicros / 1e6 << "; packing "
				<< std::chrono::duration<double>(Clock::now() - packStarted).count() << " s)\n";
		}
		return atlas;
	}

	//--- baking ---

	namespace {
		//Raise this whenever the code that builds atlases changes what it produces (background removal, frame detection,
		//scaling, packing): the baked file can't tell that the program changed, only that its inputs did.
		constexpr int ATLAS_BAKE_VERSION = 1;
		constexpr const char* BAKE_MAGIC = "NRDATLAS";

		//what a baked atlas was made from, as one string: every value that goes into the build, and the size and
		//modification time of every source image
		std::string bakeFingerprint(const std::vector<SpriteSource>& sources, float unitsPerStandingHeight, const SpriteAtlasSettings& settings)
		{
			std::ostringstream text;
			text << std::setprecision(9) << "v" << ATLAS_BAKE_VERSION << "|" << unitsPerStandingHeight << "|" << settings.standingHeight << "|" << settings.padding << "|" << settings.maxWidth;
			for (const SpriteSource& source : sources) {
				text << "|" << source.name << "," << source.path << "," << source.frames << "," << source.referenceFrame << "," << source.referenceHeight << "," << source.scale
					<< "," << source.removeBackground << "," << source.ignoreBlueForFeet << "," << source.sharedGround << "," << source.mergeDistance;
				std::error_code error;
				const auto size = std::filesystem::file_size(source.path, error);
				const auto time = std::filesystem::last_write_time(source.path, error);
				text << "," << (error ? 0 : static_cast<unsigned long long>(size)) << "," << (error ? 0 : static_cast<long long>(time.time_since_epoch().count()));
			}
			//a short hash is enough to tell "same" from "different": FNV-1a over the whole string
			std::uint64_t hash = 1469598103934665603ull;
			for (const char c : text.str()) {
				hash ^= static_cast<unsigned char>(c);
				hash *= 1099511628211ull;
			}
			std::ostringstream out;
			out << std::hex << hash << "-" << std::dec << text.str().size();
			return out.str();
		}

		//The pixels: each row is a series of (skip, literal) pairs of 16-bit counts, where skip is a run of pixels that are
		//completely transparent black (the empty space between frames, most of the atlas) and literal is how many pixels
		//follow as raw RGBA. A row is done when its pairs add up to the image's width.
		void packRow(const std::uint8_t* row, int width, std::vector<std::uint8_t>& out)
		{
			const auto zero = [&](int x) { return row[x * 4] == 0 && row[x * 4 + 1] == 0 && row[x * 4 + 2] == 0 && row[x * 4 + 3] == 0; };
			int x = 0;
			while (x < width) {
				int skip = 0;
				while (x + skip < width && zero(x + skip)) skip++;
				int literal = 0;
				while (x + skip + literal < width && literal < 65535 && !zero(x + skip + literal)) literal++;
				const std::uint16_t counts[2] = { static_cast<std::uint16_t>(skip), static_cast<std::uint16_t>(literal) };
				out.insert(out.end(), reinterpret_cast<const std::uint8_t*>(counts), reinterpret_cast<const std::uint8_t*>(counts) + 4);
				out.insert(out.end(), row + (x + skip) * 4, row + (x + skip + literal) * 4);
				x += skip + literal;
			}
		}

		bool saveBake(const std::string& path, const SpriteAtlas& atlas, const std::vector<SpriteSource>& sources, const std::string& fingerprint)
		{
			std::error_code error;
			const std::filesystem::path file{ path };
			if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);
			const std::string temporary = path + ".tmp";
			{
				std::ofstream out{ temporary, std::ios::binary | std::ios::trunc };
				if (!out) return false;
				out << BAKE_MAGIC << " " << ATLAS_BAKE_VERSION << "\n" << fingerprint << "\n"
					<< atlas.width << " " << atlas.height << " " << std::setprecision(9) << atlas.pixelsPerUnit << "\n" << sources.size() << "\n";
				for (const SpriteSource& source : sources) out << source.name << "\n";
				out << atlas.sheet->frames().size() << "\n";
				for (const SpriteFrame& frame : atlas.sheet->frames()) {
					out << frame.row << " " << frame.position.x << " " << frame.position.y << " " << frame.size.x << " " << frame.size.y << " "
						<< frame.pivot.x << " " << frame.pivot.y << " " << frame.pixelCount << "\n";
				}
				out << "pixels\n";
				std::vector<std::uint8_t> packed;
				packed.reserve(static_cast<size_t>(atlas.width) * atlas.height); //a guess: most of it is empty
				for (int y = 0; y < atlas.height; y++) packRow(atlas.pixels.data() + static_cast<size_t>(y) * atlas.width * 4, atlas.width, packed);
				out.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(packed.size()));
				if (!out) return false;
			}
			//the finished file replaces the old one in one step, so a crash half way never leaves a broken bake
			std::filesystem::remove(path, error);
			std::filesystem::rename(temporary, path, error);
			return !error;
		}

		//false if the file is missing, made from something else, or damaged in any way
		bool loadBake(const std::string& path, const std::string& fingerprint, const std::vector<SpriteSource>& sources, SpriteAtlas& atlas)
		{
			std::ifstream in{ path, std::ios::binary };
			if (!in) return false;
			std::string line;
			std::getline(in, line);
			if (line != std::string(BAKE_MAGIC) + " " + std::to_string(ATLAS_BAKE_VERSION)) return false;
			std::getline(in, line);
			if (line != fingerprint) return false;

			int width = 0, height = 0, sourceCount = 0;
			float pixelsPerUnit = 0.f;
			in >> width >> height >> pixelsPerUnit >> sourceCount;
			if (!in || width <= 0 || height <= 0 || width > 65535 || sourceCount != static_cast<int>(sources.size())) return false;
			std::getline(in, line);
			for (int i = 0; i < sourceCount; i++) {
				std::getline(in, line);
				if (line != sources[i].name) return false;
			}
			size_t frameCount = 0;
			in >> frameCount;
			if (!in || frameCount == 0 || frameCount > 100000) return false;
			std::vector<SpriteFrame> frames(frameCount);
			for (SpriteFrame& frame : frames) {
				in >> frame.row >> frame.position.x >> frame.position.y >> frame.size.x >> frame.size.y >> frame.pivot.x >> frame.pivot.y >> frame.pixelCount;
				if (!in || frame.row < 0 || frame.row >= sourceCount || frame.size.x <= 0 || frame.size.y <= 0 || frame.position.x < 0 || frame.position.y < 0 ||
					frame.position.x + frame.size.x > width || frame.position.y + frame.size.y > height) return false;
				frame.uvRect = { static_cast<float>(frame.position.x) / width, static_cast<float>(frame.position.y) / height,
					static_cast<float>(frame.size.x) / width, static_cast<float>(frame.size.y) / height };
			}
			std::getline(in, line);
			std::getline(in, line);
			if (line != "pixels") return false;

			const std::streampos start = in.tellg();
			in.seekg(0, std::ios::end);
			const std::streamoff length = in.tellg() - start;
			in.seekg(start);
			if (length <= 0) return false;
			std::vector<std::uint8_t> packed(static_cast<size_t>(length));
			in.read(reinterpret_cast<char*>(packed.data()), length);
			if (!in) return false;

			std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);
			const std::uint8_t* read = packed.data();
			const std::uint8_t* const end = read + packed.size();
			for (int y = 0; y < height; y++) {
				std::uint8_t* row = pixels.data() + static_cast<size_t>(y) * width * 4;
				int x = 0;
				while (x < width) {
					if (end - read < 4) return false;
					std::uint16_t counts[2];
					std::memcpy(counts, read, 4);
					read += 4;
					const int skip = counts[0], literal = counts[1];
					if (x + skip + literal > width || end - read < static_cast<std::ptrdiff_t>(literal) * 4) return false;
					std::memcpy(row + (x + skip) * 4, read, static_cast<size_t>(literal) * 4);
					read += static_cast<std::ptrdiff_t>(literal) * 4;
					x += skip + literal;
				}
			}
			if (read != end) return false; //left over: not the file it claims to be

			std::vector<std::uint8_t> mask(static_cast<size_t>(width) * height);
			{
				const std::uint8_t* source = pixels.data();
				std::uint8_t* opaque = mask.data();
				for (size_t i = 0; i < mask.size(); i++) opaque[i] = source[i * 4 + 3] >= 128 ? 1 : 0;
			}
			atlas = SpriteAtlas{};
			atlas.width = width;
			atlas.height = height;
			atlas.pixelsPerUnit = pixelsPerUnit;
			atlas.pixels = std::move(pixels);
			for (int i = 0; i < sourceCount; i++) atlas.rowOf[sources[i].name] = i;
			atlas.sheet = std::make_shared<SpriteSheet>(SpriteSheet::fromFrames({ width, height }, std::move(frames), std::move(mask)));
			atlas.fromCache = true;
			return true;
		}
	}

	SpriteAtlas loadOrBuildSpriteAtlas(const std::vector<SpriteSource>& sources, float unitsPerStandingHeight, const SpriteAtlasSettings& settings, const std::string& cachePath)
	{
		const auto started = std::chrono::steady_clock::now();
		const std::string fingerprint = bakeFingerprint(sources, unitsPerStandingHeight, settings);
		SpriteAtlas atlas;
		if (loadBake(cachePath, fingerprint, sources, atlas)) {
			if (settings.describe) std::cout << "atlas loaded from " << cachePath << " in " << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << " s\n";
			return atlas;
		}

		atlas = buildSpriteAtlas(sources, unitsPerStandingHeight, settings);
		const bool saved = saveBake(cachePath, atlas, sources, fingerprint);
		if (settings.describe || !saved) {
			std::cout << (saved ? "atlas baked to " : "couldn't save the baked atlas to ") << cachePath << " (took " << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << " s)\n";
		}
		return atlas;
	}
}