#include "engine/sprite/background_removal.hpp"

//std
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

//The loops index raw pointers: checked std::vector access makes Debug builds several times slower, and this runs
//over every pixel of every sprite sheet at startup.

namespace nrd {

	namespace {
		//min filter of a 0/1 mask over a (2r+1) window along one axis; outside the image counts as set, so the
		//border isn't eroded away and background can still flow in from it
		void erodeAxis(const std::uint8_t* in, std::uint8_t* out, int width, int height, int radius, bool horizontal)
		{
			const int lines = horizontal ? height : width;
			const int length = horizontal ? width : height;
			const int step = horizontal ? 1 : width;
			const int lineStep = horizontal ? width : 1;
			for (int line = 0; line < lines; line++) {
				const std::uint8_t* src = in + static_cast<std::ptrdiff_t>(line) * lineStep;
				std::uint8_t* dst = out + static_cast<std::ptrdiff_t>(line) * lineStep;
				//count of clear pixels in the window, kept as it slides
				int clear = 0;
				for (int i = 0; i <= std::min(radius, length - 1); i++) clear += src[static_cast<std::ptrdiff_t>(i) * step] ? 0 : 1;
				for (int i = 0; i < length; i++) {
					dst[static_cast<std::ptrdiff_t>(i) * step] = clear == 0 ? 1 : 0;
					const int leaving = i - radius;
					const int entering = i + radius + 1;
					if (leaving >= 0) clear -= src[static_cast<std::ptrdiff_t>(leaving) * step] ? 0 : 1;
					if (entering < length) clear += src[static_cast<std::ptrdiff_t>(entering) * step] ? 0 : 1;
				}
			}
		}

		//max filter of a 0/1 mask over a (2r+1) window along one axis
		void dilateAxis(const std::uint8_t* in, std::uint8_t* out, int width, int height, int radius, bool horizontal)
		{
			const int lines = horizontal ? height : width;
			const int length = horizontal ? width : height;
			const int step = horizontal ? 1 : width;
			const int lineStep = horizontal ? width : 1;
			for (int line = 0; line < lines; line++) {
				const std::uint8_t* src = in + static_cast<std::ptrdiff_t>(line) * lineStep;
				std::uint8_t* dst = out + static_cast<std::ptrdiff_t>(line) * lineStep;
				int set = 0;
				for (int i = 0; i <= std::min(radius, length - 1); i++) set += src[static_cast<std::ptrdiff_t>(i) * step] ? 1 : 0;
				for (int i = 0; i < length; i++) {
					dst[static_cast<std::ptrdiff_t>(i) * step] = set > 0 ? 1 : 0;
					const int leaving = i - radius;
					const int entering = i + radius + 1;
					if (leaving >= 0) set -= src[static_cast<std::ptrdiff_t>(leaving) * step] ? 1 : 0;
					if (entering < length) set += src[static_cast<std::ptrdiff_t>(entering) * step] ? 1 : 0;
				}
			}
		}

		//4-connected flood fill of `mask` pixels from `seeds`; `region` receives the filled pixels as 1
		void floodFill(const std::uint8_t* mask, std::uint8_t* region, int width, int height, std::vector<int>& stack)
		{
			while (!stack.empty()) {
				const int index = stack.back();
				stack.pop_back();
				const int x = index % width;
				const int y = index / width;
				const int neighbours[4] = { x > 0 ? index - 1 : -1, x + 1 < width ? index + 1 : -1, y > 0 ? index - width : -1, y + 1 < height ? index + width : -1 };
				for (const int n : neighbours) {
					if (n >= 0 && mask[n] && !region[n]) {
						region[n] = 1;
						stack.push_back(n);
					}
				}
			}
		}

	}

	void removeFlatBackground(std::uint8_t* rgba, int width, int height, const BackgroundRemovalSettings& settings)
	{
		const std::ptrdiff_t pixelCount = static_cast<std::ptrdiff_t>(width) * height;
		if (pixelCount == 0) return;

		//the background colour: the average of a small block in each corner
		int block = std::min(6, std::min(width, height));
		long sum[3] = { 0, 0, 0 };
		long samples = 0;
		for (int corner = 0; corner < 4; corner++) {
			const int x0 = (corner & 1) ? width - block : 0;
			const int y0 = (corner & 2) ? height - block : 0;
			for (int y = y0; y < y0 + block; y++) {
				for (int x = x0; x < x0 + block; x++) {
					const std::uint8_t* p = rgba + (static_cast<std::ptrdiff_t>(y) * width + x) * 4;
					sum[0] += p[0]; sum[1] += p[1]; sum[2] += p[2];
					samples++;
				}
			}
		}
		const int background[3] = { static_cast<int>(sum[0] / samples), static_cast<int>(sum[1] / samples), static_cast<int>(sum[2] / samples) };

		//1. how far every pixel is from the background colour, and which are close enough to count as it
		std::vector<std::uint8_t> distance(pixelCount);
		std::vector<std::uint8_t> candidate(pixelCount);
		for (std::ptrdiff_t i = 0; i < pixelCount; i++) {
			const std::uint8_t* p = rgba + i * 4;
			const int d = std::max({ std::abs(p[0] - background[0]), std::abs(p[1] - background[1]), std::abs(p[2] - background[2]) });
			distance[i] = static_cast<std::uint8_t>(std::min(d, 255));
			candidate[i] = d <= settings.tolerance ? 1 : 0;
		}

		//2. the background that reaches the border through a corridor wide enough to be real: erode the candidates,
		//flood from the border over what's left, then grow the result back out within the candidates
		const int radius = std::max(0, settings.corridor);
		std::vector<std::uint8_t> eroded(pixelCount), scratch(pixelCount);
		erodeAxis(candidate.data(), scratch.data(), width, height, radius, true);
		erodeAxis(scratch.data(), eroded.data(), width, height, radius, false);

		std::vector<std::uint8_t> reached(pixelCount, 0);
		std::vector<int> stack;
		const auto seed = [&](int x, int y) {
			const int index = y * width + x;
			if (eroded[index] && !reached[index]) {
				reached[index] = 1;
				stack.push_back(index);
			}
		};
		for (int x = 0; x < width; x++) { seed(x, 0); seed(x, height - 1); }
		for (int y = 0; y < height; y++) { seed(0, y); seed(width - 1, y); }
		floodFill(eroded.data(), reached.data(), width, height, stack);

		//enclosed pockets of pure background, if asked for
		if (settings.pocketArea > 0) {
			std::vector<std::uint8_t> pocket(pixelCount, 0);
			std::vector<int> members;
			for (std::ptrdiff_t start = 0; start < pixelCount; start++) {
				if (!eroded[start] || reached[start] || pocket[start]) continue;
				members.clear();
				pocket[start] = 1;
				stack.push_back(static_cast<int>(start));
				while (!stack.empty()) {
					const int index = stack.back();
					stack.pop_back();
					members.push_back(index);
					const int x = index % width;
					const int y = index / width;
					const int neighbours[4] = { x > 0 ? index - 1 : -1, x + 1 < width ? index + 1 : -1, y > 0 ? index - width : -1, y + 1 < height ? index + width : -1 };
					for (const int n : neighbours) {
						if (n >= 0 && eroded[n] && !reached[n] && !pocket[n]) {
							pocket[n] = 1;
							stack.push_back(n);
						}
					}
				}
				if (static_cast<int>(members.size()) >= settings.pocketArea) {
					for (const int index : members) reached[index] = 1;
				}
			}
		}

		std::vector<std::uint8_t> background_(pixelCount);
		dilateAxis(reached.data(), scratch.data(), width, height, radius, true);
		dilateAxis(scratch.data(), background_.data(), width, height, radius, false);
		for (std::ptrdiff_t i = 0; i < pixelCount; i++) background_[i] = (background_[i] && candidate[i]) ? 1 : 0;

		//3. alpha: none on the background; a ramp across the thin band along the outline; solid everywhere else
		std::vector<std::uint8_t> nearBackground(pixelCount);
		dilateAxis(background_.data(), scratch.data(), width, height, 2, true);
		dilateAxis(scratch.data(), nearBackground.data(), width, height, 2, false);
		const int ramp = std::max(1, settings.softEdge - settings.tolerance);
		for (std::ptrdiff_t i = 0; i < pixelCount; i++) {
			std::uint8_t& alpha = rgba[i * 4 + 3];
			if (background_[i]) {
				alpha = 0;
			}
			else if (nearBackground[i] && distance[i] < settings.softEdge) {
				alpha = static_cast<std::uint8_t>(std::clamp((distance[i] - settings.tolerance) * 255 / ramp, 0, 255));
			}
			else {
				alpha = 255;
			}
		}

		//4. A partly transparent pixel on the outline is the sprite mixed with the white background: replace its colour
		//with that of the solid pixels around it (a dark outline pixel is solid and real, so it stays as it is).
		//Fully transparent pixels keep whatever colour they have; they are never drawn.
		const int reach = std::max(1, settings.bleed);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				std::uint8_t* p = rgba + (static_cast<std::ptrdiff_t>(y) * width + x) * 4;
				if (p[3] == 0 || p[3] == 255) continue;
				float sum[3] = { 0.f, 0.f, 0.f };
				float weight = 0.f;
				for (int dy = -reach; dy <= reach; dy++) {
					const int yy = y + dy;
					if (yy < 0 || yy >= height) continue;
					for (int dx = -reach; dx <= reach; dx++) {
						const int xx = x + dx;
						if (xx < 0 || xx >= width) continue;
						const std::uint8_t* q = rgba + (static_cast<std::ptrdiff_t>(yy) * width + xx) * 4;
						if (q[3] != 255) continue;
						const float w = 1.f / (1.f + dx * dx + dy * dy);
						sum[0] += q[0] * w; sum[1] += q[1] * w; sum[2] += q[2] * w;
						weight += w;
					}
				}
				if (weight > 0.f) {
					p[0] = static_cast<std::uint8_t>(sum[0] / weight + 0.5f);
					p[1] = static_cast<std::uint8_t>(sum[1] / weight + 0.5f);
					p[2] = static_cast<std::uint8_t>(sum[2] / weight + 0.5f);
				}
			}
		}
	}
	void bleedColour(std::uint8_t* rgba, int width, int height, int passes)
	{
		//breadth first outwards from the opaque pixels that border a transparent one: each ring copies its colour into
		//the transparent neighbours it finds, so only pixels near a sprite are ever touched
		const std::ptrdiff_t pixelCount = static_cast<std::ptrdiff_t>(width) * height;
		std::vector<std::uint8_t> haveStorage(pixelCount);
		std::uint8_t* have = haveStorage.data();
		for (std::ptrdiff_t i = 0; i < pixelCount; i++) have[i] = rgba[i * 4 + 3] > 0 ? 1 : 0;

		std::vector<int> frontier, next;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				const int index = y * width + x;
				if (!have[index]) continue;
				if ((x > 0 && !have[index - 1]) || (x + 1 < width && !have[index + 1]) || (y > 0 && !have[index - width]) || (y + 1 < height && !have[index + width])) {
					frontier.push_back(index);
				}
			}
		}
		for (int pass = 0; pass < passes && !frontier.empty(); pass++) {
			next.clear();
			for (const int index : frontier) {
				const int x = index % width;
				const int y = index / width;
				const int neighbours[4] = { x > 0 ? index - 1 : -1, x + 1 < width ? index + 1 : -1, y > 0 ? index - width : -1, y + 1 < height ? index + width : -1 };
				for (const int n : neighbours) {
					if (n < 0 || have[n]) continue;
					std::uint8_t* dst = rgba + static_cast<std::ptrdiff_t>(n) * 4;
					const std::uint8_t* src = rgba + static_cast<std::ptrdiff_t>(index) * 4;
					dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
					have[n] = 2; //coloured, but still transparent: don't count as a source until the next ring
					next.push_back(n);
				}
			}
			for (const int index : next) have[index] = 1;
			frontier.swap(next);
		}	}
}