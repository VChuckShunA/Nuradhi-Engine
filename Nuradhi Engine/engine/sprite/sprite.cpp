#include "engine/sprite/sprite.hpp"

//libs
#include <stb_image.h>

//std
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace nrd {

	namespace {
		//a connected group of opaque pixels
		struct Island {
			glm::ivec2 min{ std::numeric_limits<int>::max() };
			glm::ivec2 max{ std::numeric_limits<int>::min() };
			int pixelCount = 0;
		};
	}

	SpriteSheet SpriteSheet::loadFromFile(const std::string& imagePath, const SpriteSheetSettings& settings)
	{
		int width = 0, height = 0, channels = 0;
		std::unique_ptr<stbi_uc, void (*)(void*)> pixels{
			stbi_load(imagePath.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free };
		if (!pixels) {
			throw std::runtime_error("Failed to load sprite sheet " + imagePath + ": " + stbi_failure_reason());
		}
		return fromPixels(pixels.get(), width, height, settings);
	}

	SpriteSheet SpriteSheet::fromPixels(const uint8_t* rgba, int width, int height, const SpriteSheetSettings& settings)
	{
		SpriteSheet sheet{};
		sheet.size = { width, height };
		const int pixelCount = width * height;

		//1. Opaque or blank. An opaque top-left corner means the sheet has no transparency and uses a flat
		//background colour instead, so anything that isn't that colour counts as sprite.
		//(These loops index raw pointers: bounds-checked std::vector access makes Debug builds many times slower.)
		const uint8_t* background = rgba;
		const bool colourKeyed = background[3] > settings.alphaThreshold;
		std::vector<uint8_t> opaque(pixelCount, 0);
		uint8_t* opaquePixels = opaque.data();
		for (int i = 0; i < pixelCount; i++) {
			const uint8_t* pixel = rgba + static_cast<size_t>(i) * 4;
			if (colourKeyed) {
				opaquePixels[i] = std::abs(pixel[0] - background[0]) > settings.colourKeyTolerance ||
					std::abs(pixel[1] - background[1]) > settings.colourKeyTolerance ||
					std::abs(pixel[2] - background[2]) > settings.colourKeyTolerance;
			}
			else {
				opaquePixels[i] = pixel[3] > settings.alphaThreshold;
			}
		}

		//2. Flood fill islands of opaque pixels. Pixels within mergeDistance of each other join the same island,
		//which keeps strands of hair, weapons and effects with the sprite they belong to.
		std::vector<int> islandOf(pixelCount, -1);
		int* label = islandOf.data();
		std::vector<Island> islands;
		std::vector<int> pending;
		const int reach = std::max(1, settings.mergeDistance);
		for (int start = 0; start < pixelCount; start++) {
			if (!opaquePixels[start] || label[start] != -1) continue;

			const int id = static_cast<int>(islands.size());
			Island island{};
			label[start] = id;
			pending.push_back(start);
			while (!pending.empty()) {
				const int index = pending.back();
				pending.pop_back();
				const glm::ivec2 pixel{ index % width, index / width };
				island.min = glm::min(island.min, pixel);
				island.max = glm::max(island.max, pixel);
				island.pixelCount++;

				const int y0 = std::max(0, pixel.y - reach), y1 = std::min(height - 1, pixel.y + reach);
				const int x0 = std::max(0, pixel.x - reach), x1 = std::min(width - 1, pixel.x + reach);
				for (int y = y0; y <= y1; y++) {
					const int row = y * width;
					for (int x = x0; x <= x1; x++) {
						const int neighbour = row + x;
						if (opaquePixels[neighbour] && label[neighbour] == -1) {
							label[neighbour] = id;
							pending.push_back(neighbour);
						}
					}
				}
			}
			islands.push_back(island);
		}
		//2a. A sheet with fewer separate pieces than frames has frames that touch (swords crossing into the next frame,
		//an arm reaching it). The frames sit in equal cells, so a piece several cells wide is cut into that many, each
		//cut at the emptiest column near the cell boundary.
		if (settings.expectedFrames > 0) {
			const float cell = static_cast<float>(width) / settings.expectedFrames;
			const int bigPixels = std::max(settings.minPixels, 600);
			const auto bigCount = [&]() {
				int count = 0;
				for (const Island& island : islands) count += island.pixelCount >= bigPixels ? 1 : 0;
				return count;
			};
			int deficit = settings.expectedFrames - bigCount();
			while (deficit > 0) {
				int widest = -1;
				float widestCells = 1.5f;
				for (int id = 0; id < static_cast<int>(islands.size()); id++) {
					if (islands[id].pixelCount < bigPixels) continue;
					const float cells = (islands[id].max.x - islands[id].min.x + 1) / cell;
					if (cells > widestCells) { widestCells = cells; widest = id; }
				}
				if (widest < 0) break;
				const int parts = std::min(std::max(2, static_cast<int>(std::lround(widestCells))), deficit + 1);
				const Island whole = islands[widest];
				const int span = whole.max.x - whole.min.x + 1;

				//ink per column of this piece, and the cuts
				std::vector<int> ink(span, 0);
				for (int y = whole.min.y; y <= whole.max.y; y++) {
					for (int x = whole.min.x; x <= whole.max.x; x++) {
						if (islandOf[y * width + x] == widest) ink[x - whole.min.x]++;
					}
				}
				std::vector<int> cuts;
				for (int part = 1; part < parts; part++) {
					const int target = static_cast<int>(span * static_cast<float>(part) / parts);
					const int window = static_cast<int>(cell * 0.3f);
					int best = std::clamp(target, 1, span - 1);
					long bestScore = std::numeric_limits<long>::max();
					for (int x = std::max(1, target - window); x <= std::min(span - 1, target + window); x++) {
						const long score = static_cast<long>(ink[x]) * 8 + std::abs(x - target); //emptiest column, nearest the boundary on ties
						if (score < bestScore) { bestScore = score; best = x; }
					}
					cuts.push_back(whole.min.x + best);
				}

				//the new pieces take over their columns' pixels
				const int firstNew = static_cast<int>(islands.size());
				for (int part = 1; part < parts; part++) islands.push_back(Island{});
				for (int y = whole.min.y; y <= whole.max.y; y++) {
					for (int x = whole.min.x; x <= whole.max.x; x++) {
						const int pixel = y * width + x;
						if (islandOf[pixel] != widest) continue;
						int part = 0;
						while (part < static_cast<int>(cuts.size()) && x >= cuts[part]) part++;
						const int id = part == 0 ? widest : firstNew + part - 1;
						islandOf[pixel] = id;
					}
				}
				islands[widest] = Island{};
				for (int y = whole.min.y; y <= whole.max.y; y++) {
					for (int x = whole.min.x; x <= whole.max.x; x++) {
						const int id = islandOf[y * width + x];
						if (id != widest && id < firstNew) continue;
						Island& island = islands[id];
						island.min = glm::min(island.min, glm::ivec2{ x, y });
						island.max = glm::max(island.max, glm::ivec2{ x, y });
						island.pixelCount++;
					}
				}
				deficit -= parts - 1;
			}
			if (deficit > 0) std::cerr << "sprite sheet: " << deficit << " frames short and no piece wide enough to split\n";
		}

		//2b. A sheet that says how many frames it has: absorb the smallest islands into their nearest neighbours until
		//that many are left. `finalOf` maps every island to the one it ended up in.
		std::vector<int> finalOf(islands.size());
		for (int id = 0; id < static_cast<int>(islands.size()); id++) finalOf[id] = id;
		std::vector<int> alive;
		for (int id = 0; id < static_cast<int>(islands.size()); id++) {
			if (settings.expectedFrames > 0 || islands[id].pixelCount >= settings.minPixels) alive.push_back(id);
		}
		if (settings.expectedFrames > 0) {
			const auto gapBetween = [](const Island& a, const Island& b) {
				const int dx = std::max({ 0, a.min.x - b.max.x, b.min.x - a.max.x });
				const int dy = std::max({ 0, a.min.y - b.max.y, b.min.y - a.max.y });
				return static_cast<long long>(dx) * dx + static_cast<long long>(dy) * dy;
			};
			while (static_cast<int>(alive.size()) > settings.expectedFrames) {
				const auto smallest = std::min_element(alive.begin(), alive.end(), [&](int a, int b) { return islands[a].pixelCount < islands[b].pixelCount; });
				const int from = *smallest;
				alive.erase(smallest);
				int into = alive.front();
				for (const int candidate : alive) {
					if (gapBetween(islands[from], islands[candidate]) < gapBetween(islands[from], islands[into])) into = candidate;
				}
				islands[into].min = glm::min(islands[into].min, islands[from].min);
				islands[into].max = glm::max(islands[into].max, islands[from].max);
				islands[into].pixelCount += islands[from].pixelCount;
				for (int& target : finalOf) {
					if (target == from) target = into;
				}
			}
			if (static_cast<int>(alive.size()) < settings.expectedFrames) {
				std::cerr << "sprite sheet: expected " << settings.expectedFrames << " frames but only found " << alive.size()
					<< " (frames touching each other?)\n";
			}
		}

		//the lowest pixel of each island that counts as a foot, with blue effects optionally left out
		const auto isBlue = [&](int pixel) {
			const uint8_t* p = rgba + static_cast<size_t>(pixel) * 4;
			return p[2] > 140 && p[2] - p[0] > 60;
		};
		std::vector<int> feetBottom(islands.size(), std::numeric_limits<int>::min());
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				const int pixel = y * width + x;
				if (islandOf[pixel] < 0 || !opaque[pixel]) continue;
				if (settings.ignoreBlueForFeet && isBlue(pixel)) continue;
				int& bottom = feetBottom[finalOf[islandOf[pixel]]];
				bottom = std::max(bottom, y);
			}
		}

		//3. Every island left becomes a frame
		std::vector<SpriteFrame> found;
		for (const int id : alive) {
			const Island& island = islands[id];

			SpriteFrame frame{};
			frame.position = island.min;
			frame.size = island.max - island.min + 1;
			frame.pixelCount = island.pixelCount;
			frame.uvRect = {
				static_cast<float>(frame.position.x) / width, static_cast<float>(frame.position.y) / height,
				static_cast<float>(frame.size.x) / width, static_cast<float>(frame.size.y) / height };

			//pivot: horizontal centre of the island's lowest pixels (the feet), on the bottom edge. Anchoring there
			//keeps a character planted when a pose sticks an arm or leg out and widens the frame.
			const int feet = feetBottom[id] == std::numeric_limits<int>::min() ? island.max.y : feetBottom[id];
			const int feetHeight = feet - island.min.y + 1;
			const int band = std::max(2, static_cast<int>(std::ceil(feetHeight * settings.footBand)));
			double sumX = 0.0;
			int feetPixels = 0;
			for (int y = std::max(island.min.y, feet - band + 1); y <= feet; y++) {
				for (int x = island.min.x; x <= island.max.x; x++) {
					const int pixel = y * width + x;
					if (islandOf[pixel] < 0 || finalOf[islandOf[pixel]] != id) continue;
					if (settings.ignoreBlueForFeet && isBlue(pixel)) continue;
					sumX += x + 0.5;
					feetPixels++;
				}
			}
			const float pivotX = feetPixels > 0 ? static_cast<float>(sumX / feetPixels) - frame.position.x : frame.size.x * 0.5f;
			frame.pivot = { pivotX, static_cast<float>(feet + 1 - frame.position.y) };
			frame.island = id;
			found.push_back(frame);
		}
		sheet.opaqueMask = opaque;

		//4. Reading order. Frames whose vertical extents overlap by at least half the shorter one share a row.
		std::sort(found.begin(), found.end(), [](const SpriteFrame& a, const SpriteFrame& b) { return a.position.y < b.position.y; });
		struct Row {
			int top, bottom; //pixel rows, bottom exclusive
			std::vector<int> members; //indices into found
		};
		std::vector<Row> rows;
		for (int i = 0; i < static_cast<int>(found.size()); i++) {
			const int top = found[i].position.y;
			const int bottom = top + found[i].size.y;
			Row* home = nullptr;
			for (Row& row : rows) {
				const int overlap = std::min(bottom, row.bottom) - std::max(top, row.top);
				const int shorter = std::min(bottom - top, row.bottom - row.top);
				if (overlap * 2 >= shorter) {
					home = &row;
					break;
				}
			}
			if (!home) {
				rows.push_back({ top, bottom, {} });
				home = &rows.back();
			}
			home->members.push_back(i);
			home->top = std::min(home->top, top);
			home->bottom = std::max(home->bottom, bottom);
		}
		std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.top < b.top; });

		for (int r = 0; r < static_cast<int>(rows.size()); r++) {
			std::vector<int>& members = rows[r].members;
			std::sort(members.begin(), members.end(), [&](int a, int b) { return found[a].position.x < found[b].position.x; });
			std::vector<int> rowFrames;
			for (int member : members) {
				SpriteFrame frame = found[member];
				frame.row = r;
				rowFrames.push_back(static_cast<int>(sheet.detectedFrames.size()));
				sheet.detectedFrames.push_back(frame);
			}
			sheet.detectedRows.push_back(std::move(rowFrames));
		}
		//every frame in a row stands on that row's ground line: the median of the frames' feet
		if (settings.sharedGround) {
			for (const std::vector<int>& rowFrames : sheet.detectedRows) {
				std::vector<float> grounds;
				for (const int index : rowFrames) grounds.push_back(sheet.detectedFrames[index].position.y + sheet.detectedFrames[index].pivot.y);
				std::nth_element(grounds.begin(), grounds.begin() + grounds.size() / 2, grounds.end());
				const float ground = grounds[grounds.size() / 2];
				for (const int index : rowFrames) sheet.detectedFrames[index].pivot.y = ground - sheet.detectedFrames[index].position.y;
			}
		}
		if (settings.keepFrameMap) {
			std::vector<int> frameOfIsland(islands.size(), -1);
			for (int i = 0; i < static_cast<int>(sheet.detectedFrames.size()); i++) frameOfIsland[sheet.detectedFrames[i].island] = i;
			sheet.frameMap.assign(pixelCount, -1);
			for (int pixel = 0; pixel < pixelCount; pixel++) {
				if (islandOf[pixel] >= 0) sheet.frameMap[pixel] = frameOfIsland[finalOf[islandOf[pixel]]];
			}
		}
		return sheet;
	}

	SpriteSheet SpriteSheet::fromFrames(glm::ivec2 imageSize, std::vector<SpriteFrame> frames, std::vector<uint8_t> opaqueMask)
	{
		SpriteSheet sheet{};
		sheet.size = imageSize;
		sheet.opaqueMask = std::move(opaqueMask);
		for (SpriteFrame& frame : frames) {
			if (frame.row < 0) throw std::invalid_argument("A frame given to SpriteSheet::fromFrames needs a row");
			if (frame.row >= static_cast<int>(sheet.detectedRows.size())) sheet.detectedRows.resize(frame.row + 1);
			sheet.detectedRows[frame.row].push_back(static_cast<int>(sheet.detectedFrames.size()));
			sheet.detectedFrames.push_back(frame);
		}
		return sheet;
	}

	int SpriteSheet::frameAt(int row, int column) const
	{
		if (row < 0 || row >= static_cast<int>(detectedRows.size()) ||
			column < 0 || column >= static_cast<int>(detectedRows[row].size())) {
			throw std::out_of_range("Sprite sheet has no frame at row " + std::to_string(row) + ", column " + std::to_string(column));
		}
		return detectedRows[row][column];
	}

	void SpriteSheet::addAnimation(const SpriteAnimation& animation)
	{
		if (animation.frames.empty()) throw std::invalid_argument("Sprite animation " + animation.name + " has no frames");
		if (animation.framesPerSecond <= 0.f) throw std::invalid_argument("Sprite animation " + animation.name + " needs a positive frame rate");
		for (int frame : animation.frames) {
			if (frame < 0 || frame >= static_cast<int>(detectedFrames.size())) {
				throw std::out_of_range("Sprite animation " + animation.name + " uses frame " + std::to_string(frame) + ", which the sheet doesn't have");
			}
		}
		animations[animation.name] = animation;
	}

	void SpriteSheet::addRowAnimation(const std::string& name, int row, int firstColumn, int count, float framesPerSecond, bool loop)
	{
		SpriteAnimation animation{ name, {}, framesPerSecond, loop };
		for (int column = firstColumn; column < firstColumn + count; column++) {
			animation.frames.push_back(frameAt(row, column));
		}
		addAnimation(animation);
	}

	void SpriteSheet::setAnchors(int frame, const std::string& name, std::vector<glm::vec2> points)
	{
		if (frame < 0 || frame >= static_cast<int>(detectedFrames.size())) throw std::out_of_range("setAnchors: the sheet has no frame " + std::to_string(frame));
		if (points.empty()) anchorPoints[name].erase(frame);
		else anchorPoints[name][frame] = std::move(points);
	}

	const std::vector<glm::vec2>* SpriteSheet::anchors(int frame, const std::string& name) const
	{
		const auto named = anchorPoints.find(name);
		if (named == anchorPoints.end()) return nullptr;
		const auto found = named->second.find(frame);
		return found == named->second.end() ? nullptr : &found->second;
	}

	const SpriteAnimation* SpriteSheet::findAnimation(const std::string& name) const
	{
		const auto found = animations.find(name);
		return found == animations.end() ? nullptr : &found->second;
	}

	void SpriteSheet::describe(std::ostream& out) const
	{
		out << "sprite sheet " << size.x << "x" << size.y << ": " << detectedFrames.size() << " frames in " << detectedRows.size() << " rows\n";
		for (int r = 0; r < static_cast<int>(detectedRows.size()); r++) {
			for (int column = 0; column < static_cast<int>(detectedRows[r].size()); column++) {
				const int index = detectedRows[r][column];
				const SpriteFrame& frame = detectedFrames[index];
				out << "frame " << index << " row " << r << " col " << column
					<< " at " << frame.position.x << "," << frame.position.y
					<< " size " << frame.size.x << "x" << frame.size.y
					<< " pivot " << frame.pivot.x << "," << frame.pivot.y
					<< " pixels " << frame.pixelCount << "\n";
			}
		}
	}

	SpriteAnimator::SpriteAnimator(std::shared_ptr<const SpriteSheet> sheet) : spriteSheet{ std::move(sheet) }
	{
		assert(spriteSheet && !spriteSheet->frames().empty() && "A sprite animator needs a sheet with at least one frame");
	}

	void SpriteAnimator::play(const std::string& name, bool restart)
	{
		const SpriteAnimation* next = spriteSheet->findAnimation(name);
		if (!next) throw std::invalid_argument("Unknown sprite animation: " + name);
		if (next == animation && !restart) return;

		animation = next;
		frameInAnimation = 0;
		frameTimer = 0.f;
	}

	void SpriteAnimator::update(float dt)
	{
		if (!animation) return;

		frameTimer += dt;
		const float frameDuration = 1.f / animation->framesPerSecond;
		while (frameTimer >= frameDuration) {
			frameTimer -= frameDuration;
			if (frameInAnimation + 1 < static_cast<int>(animation->frames.size())) {
				frameInAnimation++;
			}
			else if (animation->loop) {
				frameInAnimation = 0;
			}
			else {
				frameTimer = 0.f; //hold the last frame
				break;
			}
		}
	}

	int SpriteAnimator::currentFrameIndex() const
	{
		return animation ? animation->frames[frameInAnimation] : 0;
	}

	const SpriteFrame& SpriteAnimator::currentFrame() const
	{
		return spriteSheet->frames()[currentFrameIndex()];
	}

	const std::string& SpriteAnimator::currentAnimation() const
	{
		static const std::string none{};
		return animation ? animation->name : none;
	}

	bool SpriteAnimator::finished() const
	{
		return animation && !animation->loop && frameInAnimation + 1 == static_cast<int>(animation->frames.size());
	}

	Sprite::Sprite(std::shared_ptr<const SpriteSheet> sheet, TextureHandle texture, float pixelsPerUnit)
		: animator{ std::move(sheet) }, texture{ texture }, pixelsPerUnit{ pixelsPerUnit }
	{
		assert(pixelsPerUnit > 0.f && "pixelsPerUnit must be positive");
	}

	Transform Sprite::frameTransform(const glm::vec3& position, const Aabb* body) const
	{
		const SpriteFrame& frame = animator.currentFrame();

		//the quad's origin is its bottom-centre, so shift it until the frame's pivot lands on position
		float pivotX = (frame.pivot.x - frame.size.x * 0.5f) / pixelsPerUnit;
		if (flipX) pivotX = -pivotX;
		const float pivotHeight = (frame.size.y - frame.pivot.y) / pixelsPerUnit; //distance above the bottom edge (-y is up)

		Transform transform{};
		transform.position = position + glm::vec3{ -pivotX, pivotHeight, 0.f };
		transform.scale = { frame.size.x / pixelsPerUnit, frame.size.y / pixelsPerUnit, 1.f };

		if (keepBackInsideCollider && body) {
			const float halfWidth = transform.scale.x * 0.5f;
			if (!flipX) {
				const float backEdge = transform.position.x - halfWidth; //facing right: the back is the left edge
				if (backEdge < body->min.x) transform.position.x += body->min.x - backEdge;
			}
			else {
				const float backEdge = transform.position.x + halfWidth; //facing left: the back is the right edge
				if (backEdge > body->max.x) transform.position.x -= backEdge - body->max.x;
			}
		}
		return transform;
	}

	glm::vec3 Sprite::worldPoint(const glm::vec3& position, const Aabb* body, const glm::vec2& framePoint) const
	{
		//the quad's origin is its bottom-centre and it stands upright: it spans scale.x across and scale.y up from there
		const Transform quad = frameTransform(position, body);
		const SpriteFrame& frame = animator.currentFrame();
		float across = framePoint.x / static_cast<float>(frame.size.x);
		if (flipX) across = 1.f - across;
		const float down = framePoint.y / static_cast<float>(frame.size.y);
		return { quad.position.x - quad.scale.x * 0.5f + across * quad.scale.x, quad.position.y - quad.scale.y + down * quad.scale.y, position.z };
	}

	glm::vec4 Sprite::frameUvRect() const
	{
		const glm::vec4 uv = animator.currentFrame().uvRect;
		//mirroring: start at the right edge and walk left
		return flipX ? glm::vec4{ uv.x + uv.z, uv.y, -uv.z, uv.w } : uv;
	}

	void updateSpriteAnimations(ecs::Registry& registry, float dt)
	{
		registry.each<Sprite>([dt](ecs::Entity, Sprite& sprite) {
			sprite.animator.update(dt);
		});
	}
}
