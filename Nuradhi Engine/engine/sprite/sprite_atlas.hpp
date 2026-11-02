#pragma once

#include "engine/sprite/sprite.hpp"

//std
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nrd {

	//One sprite sheet image to put in an atlas. Its frames are found from the image itself, like SpriteSheet does.
	struct SpriteSource {
		std::string name; //how the game refers to it, e.g. "melee.walk"
		std::string path; //an image with a flat white-ish background, or with real transparency
		int frames = 0; //how many frames the sheet holds, so loose bits (muzzle flashes, sparks) are folded into them

		//Sheets drawn by AI come at different scales. The frame at `referenceFrame` is scaled to `referenceHeight`
		//standing heights (see SpriteAtlasSettings), which scales the whole sheet with it. Pick a frame where she
		//stands upright, and say how tall that pose is compared to a relaxed stance (a guard stance is a bit shorter).
		int referenceFrame = 0;
		float referenceHeight = 1.f;
		float scale = 1.f; //on top of that, to nudge a sheet by hand

		bool removeBackground = true; //false for images that already have transparency
		bool ignoreBlueForFeet = false; //glowing blades and effects don't count as the ground (see SpriteSheetSettings)
		bool sharedGround = false; //grounded poses all stand on one line (see SpriteSheetSettings)
		int mergeDistance = 3; //pixels apart that still count as one piece
	};

	struct SpriteAtlasSettings {
		float standingHeight = 525.f; //atlas pixels from the feet to the top of a relaxed standing frame
		int padding = 4; //empty pixels around every frame, so filtering and mipmaps can't pull in a neighbour
		int maxWidth = 8192;
		bool describe = false; //print every source's frames and scale to the console
	};

	//Every source's frames resampled to a common scale and packed into one image, with a SpriteSheet over it. Source
	//number i (in the order given) is row i of the sheet, so sheet->frameAt(i, column) is that sheet's column'th frame.
	struct SpriteAtlas {
		std::vector<std::uint8_t> pixels; //RGBA, top row first; ready for a texture
		int width = 0;
		int height = 0;
		std::shared_ptr<SpriteSheet> sheet;
		std::unordered_map<std::string, int> rowOf; //source name -> row

		int row(const std::string& name) const; //throws for an unknown name
		int frame(const std::string& name, int column) const { return sheet->frameAt(row(name), column); }
		int frameCount(const std::string& name) const { return static_cast<int>(sheet->rows()[row(name)].size()); }
		float pixelsPerUnit = 1.f; //set by buildSpriteAtlas from `unitsPerStandingHeight`
		bool fromCache = false; //loaded from a baked file by loadOrBuildSpriteAtlas instead of being built
	};

	//Builds the atlas. `unitsPerStandingHeight` is how many world units a relaxed standing frame is tall.
	SpriteAtlas buildSpriteAtlas(const std::vector<SpriteSource>& sources, float unitsPerStandingHeight, const SpriteAtlasSettings& settings = {});

	//Building the atlas takes seconds (every sheet is decoded, cleaned, analysed, scaled and packed), so the result is
	//baked: the first call builds it and saves it to `cachePath` (one file; the folder is created if needed), and later calls
	//read that file back, which takes a fraction of a second. The file is rebuilt automatically when anything it was made
	//from changes: a source image (its size or modified time), any SpriteSource or settings value, or the build itself
	//(ATLAS_BAKE_VERSION in sprite_atlas.cpp, which has to be raised by hand when the code that builds atlases changes).
	//If the file can't be written the atlas is still returned, just built again next time. Delete the file to force a rebuild.
	SpriteAtlas loadOrBuildSpriteAtlas(const std::vector<SpriteSource>& sources, float unitsPerStandingHeight, const SpriteAtlasSettings& settings, const std::string& cachePath);
}
