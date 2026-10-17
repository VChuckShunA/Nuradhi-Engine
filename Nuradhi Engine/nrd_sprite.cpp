#include "nrd_sprite.hpp"

//libs
#include <stb_image.h>

//std
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
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
		const uint8_t* background = rgba;
		const bool colourKeyed = background[3] > settings.alphaThreshold;
		std::vector<uint8_t> opaque(pixelCount, 0);
		for (int i = 0; i < pixelCount; i++) {
			const uint8_t* pixel = rgba + static_cast<size_t>(i) * 4;
			if (colourKeyed) {
				opaque[i] = std::abs(pixel[0] - background[0]) > settings.colourKeyTolerance ||
					std::abs(pixel[1] - background[1]) > settings.colourKeyTolerance ||
					std::abs(pixel[2] - background[2]) > settings.colourKeyTolerance;
			}
			else {
				opaque[i] = pixel[3] > settings.alphaThreshold;
			}
		}

		//2. Flood fill islands of opaque pixels. Pixels within mergeDistance of each other join the same island,
		//which keeps strands of hair, weapons and effects with the sprite they belong to.
		std::vector<int> islandOf(pixelCount, -1);
		std::vector<Island> islands;
		std::vector<int> pending;
		const int reach = std::max(1, settings.mergeDistance);
		for (int start = 0; start < pixelCount; start++) {
			if (!opaque[start] || islandOf[start] != -1) continue;

			const int id = static_cast<int>(islands.size());
			Island island{};
			islandOf[start] = id;
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
					for (int x = x0; x <= x1; x++) {
						const int neighbour = y * width + x;
						if (opaque[neighbour] && islandOf[neighbour] == -1) {
							islandOf[neighbour] = id;
							pending.push_back(neighbour);
						}
					}
				}
			}
			islands.push_back(island);
		}

		//3. Every island big enough to be more than dust becomes a frame
		std::vector<SpriteFrame> found;
		for (int id = 0; id < static_cast<int>(islands.size()); id++) {
			const Island& island = islands[id];
			if (island.pixelCount < settings.minPixels) continue;

			SpriteFrame frame{};
			frame.position = island.min;
			frame.size = island.max - island.min + 1;
			frame.pixelCount = island.pixelCount;
			frame.uvRect = {
				static_cast<float>(frame.position.x) / width, static_cast<float>(frame.position.y) / height,
				static_cast<float>(frame.size.x) / width, static_cast<float>(frame.size.y) / height };

			//pivot: horizontal centre of the island's lowest pixels (the feet), on the bottom edge. Anchoring there
			//keeps a character planted when a pose sticks an arm or leg out and widens the frame.
			const int band = std::max(2, static_cast<int>(std::ceil(frame.size.y * settings.footBand)));
			double sumX = 0.0;
			int feetPixels = 0;
			for (int y = std::max(island.min.y, island.max.y - band + 1); y <= island.max.y; y++) {
				for (int x = island.min.x; x <= island.max.x; x++) {
					if (islandOf[y * width + x] == id) {
						sumX += x + 0.5;
						feetPixels++;
					}
				}
			}
			const float pivotX = feetPixels > 0 ? static_cast<float>(sumX / feetPixels) - frame.position.x : frame.size.x * 0.5f;
			frame.pivot = { pivotX, static_cast<float>(frame.size.y) };
			found.push_back(frame);
		}

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

	const SpriteFrame& SpriteAnimator::currentFrame() const
	{
		const int index = animation ? animation->frames[frameInAnimation] : 0;
		return spriteSheet->frames()[index];
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

	SpriteRenderer::SpriteRenderer(std::shared_ptr<const SpriteSheet> sheet, float pixelsPerUnit)
		: animator{ std::move(sheet) }, pixelsPerUnit{ pixelsPerUnit }
	{
		assert(pixelsPerUnit > 0.f && "pixelsPerUnit must be positive");
	}

	lve::TransformComponent SpriteRenderer::frameTransform(const glm::vec3& position) const
	{
		const SpriteFrame& frame = animator.currentFrame();

		//the quad's origin is its bottom-centre, so shift it until the frame's pivot lands on position
		float pivotX = (frame.pivot.x - frame.size.x * 0.5f) / pixelsPerUnit;
		if (flipX) pivotX = -pivotX;
		const float pivotHeight = (frame.size.y - frame.pivot.y) / pixelsPerUnit; //distance above the bottom edge (-y is up)

		lve::TransformComponent transform{};
		transform.translation = position + glm::vec3{ -pivotX, pivotHeight, 0.f };
		transform.scale = { frame.size.x / pixelsPerUnit, frame.size.y / pixelsPerUnit, 1.f };
		return transform;
	}

	glm::vec4 SpriteRenderer::frameUvRect() const
	{
		const glm::vec4 uv = animator.currentFrame().uvRect;
		//mirroring: start at the right edge and walk left
		return flipX ? glm::vec4{ uv.x + uv.z, uv.y, -uv.z, uv.w } : uv;
	}

	std::unique_ptr<lve::LveModel> createSpriteQuad(lve::LveDevice& device)
	{
		const glm::vec3 white{ 1.f };
		const glm::vec3 towardsCamera{ 0.f, 0.f, -1.f };
		lve::LveModel::Builder builder{};
		builder.vertices = {
			{ { -0.5f, -1.f, 0.f }, white, towardsCamera, { 0.f, 0.f } }, //top left
			{ {  0.5f, -1.f, 0.f }, white, towardsCamera, { 1.f, 0.f } }, //top right
			{ {  0.5f,  0.f, 0.f }, white, towardsCamera, { 1.f, 1.f } }, //bottom right
			{ { -0.5f,  0.f, 0.f }, white, towardsCamera, { 0.f, 1.f } }, //bottom left
		};
		builder.indices = { 0, 1, 2, 2, 3, 0 };
		return std::make_unique<lve::LveModel>(device, builder);
	}
}
