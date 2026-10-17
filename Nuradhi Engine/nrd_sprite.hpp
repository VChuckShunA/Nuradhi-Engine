#pragma once

#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_model.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace nrd {

	//One sprite found on a sheet. Pixel coordinates have their origin at the top-left of the image.
	struct SpriteFrame {
		glm::ivec2 position{ 0 }; //top-left pixel
		glm::ivec2 size{ 0 }; //width and height in pixels
		glm::vec2 pivot{ 0.f }; //point inside the frame (frame pixels) that sits on the entity's position: centre of the feet, bottom edge
		glm::vec4 uvRect{ 0.f }; //xy = top-left, zw = size, in 0..1 texture coordinates
		int pixelCount = 0; //opaque pixels in the island
		int row = 0; //which detected row of the sheet the frame is in
	};

	struct SpriteAnimation {
		std::string name;
		std::vector<int> frames; //indices into SpriteSheet::frames()
		float framesPerSecond = 10.f;
		bool loop = true; //false holds the last frame
	};

	struct SpriteSheetSettings {
		int alphaThreshold = 16; //alpha at or below this is a blank pixel
		int colourKeyTolerance = 8; //sheets without transparency: max per-channel distance from the corner colour that is still background
		int mergeDistance = 2; //opaque pixels this close (any direction) belong to the same sprite, so loose hair and weapons stay attached
		int minPixels = 12; //islands smaller than this are dust and get dropped
		float footBand = 0.1f; //bottom fraction of a frame searched for feet when placing the pivot
	};

	//Finds every sprite on a sheet by separating opaque pixels from blank ones, then orders them in rows,
	//top to bottom and left to right, so frame indices read like the sheet does.
	class SpriteSheet {
	public:
		static SpriteSheet loadFromFile(const std::string& imagePath, const SpriteSheetSettings& settings = {});
		static SpriteSheet fromPixels(const uint8_t* rgba, int width, int height, const SpriteSheetSettings& settings = {});

		const std::vector<SpriteFrame>& frames() const { return detectedFrames; }
		const std::vector<std::vector<int>>& rows() const { return detectedRows; } //frame indices in each row
		glm::ivec2 imageSize() const { return size; }

		//frame index of the `column`th sprite in `row`; throws if there isn't one
		int frameAt(int row, int column) const;

		//registers an animation from explicit frame indices
		void addAnimation(const SpriteAnimation& animation);
		//registers an animation of `count` consecutive frames from one row, starting at `firstColumn`
		void addRowAnimation(const std::string& name, int row, int firstColumn, int count, float framesPerSecond, bool loop = true);
		const SpriteAnimation* findAnimation(const std::string& name) const;

		//prints every row and frame with its index, position, size and pivot, for authoring animations
		void describe(std::ostream& out) const;

	private:
		glm::ivec2 size{ 0 };
		std::vector<SpriteFrame> detectedFrames;
		std::vector<std::vector<int>> detectedRows;
		std::unordered_map<std::string, SpriteAnimation> animations;
	};

	//Plays one SpriteAnimation at a time from a sheet
	class SpriteAnimator {
	public:
		explicit SpriteAnimator(std::shared_ptr<const SpriteSheet> sheet);

		//switches animation; does nothing if it is already playing, unless restart is set
		void play(const std::string& name, bool restart = false);
		void update(float dt);

		const SpriteFrame& currentFrame() const;
		const std::string& currentAnimation() const;
		bool finished() const; //a non-looping animation has reached its last frame
		const SpriteSheet& sheet() const { return *spriteSheet; }

	private:
		std::shared_ptr<const SpriteSheet> spriteSheet;
		const SpriteAnimation* animation = nullptr;
		int frameInAnimation = 0;
		float frameTimer = 0.f;
	};

	//Draws an entity as the animator's current frame on a camera-facing quad
	struct SpriteRenderer {
		SpriteRenderer(std::shared_ptr<const SpriteSheet> sheet, float pixelsPerUnit);

		SpriteAnimator animator;
		float pixelsPerUnit; //sheet pixels per world unit
		bool flipX = false; //mirror horizontally, e.g. to face left with a sheet drawn facing right

		//render transform for a sprite quad (see createSpriteQuad) that puts the frame's pivot on `position`
		lve::TransformComponent frameTransform(const glm::vec3& position) const;
		glm::vec4 frameUvRect() const;
	};

	//Unit quad in the XY plane facing the camera (-z), with its bottom-centre at the origin, so it stands up
	//from wherever it's placed. Its UVs put the top of the image at the top (-y is up).
	std::unique_ptr<lve::LveModel> createSpriteQuad(lve::LveDevice& device);
}
