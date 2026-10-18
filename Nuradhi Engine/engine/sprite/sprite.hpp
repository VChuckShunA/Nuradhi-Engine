#pragma once

#include "engine/core/components.hpp"
#include "engine/ecs/registry.hpp"
#include "engine/physics/collision.hpp"
#include "engine/render/render_components.hpp"

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

	//Component: draws the entity as its animator's current frame, standing upright and facing the camera, with
	//the frame's pivot on the entity's Transform position. The texture must be the sheet's image.
	struct Sprite {
		Sprite(std::shared_ptr<const SpriteSheet> sheet, TextureHandle texture, float pixelsPerUnit);

		SpriteAnimator animator;
		TextureHandle texture;
		float pixelsPerUnit; //sheet pixels per world unit
		bool flipX = false; //mirror horizontally, e.g. to face left with a sheet drawn facing right
		//never let the frame's back edge (the left edge facing right, the right edge facing left) stick out
		//behind the entity's Collider. Poses whose lowest pixels are an outstretched foot, like a flying kick,
		//would otherwise be pushed backwards by the foot pivot and jump sideways between frames.
		bool keepBackInsideCollider = false;

		//transform for the renderer's sprite quad (a unit quad whose bottom-centre is its origin) that puts the
		//current frame's pivot on `position`; with keepBackInsideCollider and a `body`, it's then nudged forward
		//until the back edge is inside the body box
		Transform frameTransform(const glm::vec3& position, const Aabb* body = nullptr) const;
		glm::vec4 frameUvRect() const;
	};

	//advances every Sprite's animation; the engine runs it each frame
	void updateSpriteAnimations(ecs::Registry& registry, float dt);
}
