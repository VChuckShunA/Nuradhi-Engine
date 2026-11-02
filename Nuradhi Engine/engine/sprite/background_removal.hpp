#pragma once

//std
#include <cstdint>

namespace nrd {

	struct BackgroundRemovalSettings {
		int tolerance = 6; //most a channel can differ from the background colour for a pixel to count as pure background
		//Background only counts if it is at least 2 * corridor + 1 pixels wide where it flows in from the border.
		//That is what keeps white clothes opaque: a hairline gap in a sprite's outline is narrower than that, so the
		//background can't leak through it into a white shirt.
		int corridor = 2;
		int softEdge = 40; //a pixel this far from the background colour (or more) is fully opaque; closer ones fade
		//A pocket of pure background that the border can't reach (between an arm and the body, say) is removed too
		//when it is at least this many pixels. 0 keeps every enclosed pocket.
		int pocketArea = 0;
		int bleed = 3; //partly transparent edge pixels take the colour of solid ones up to this many pixels away
	};

	//Turns a flat, near-white background into transparency, in place. `rgba` is tightly packed 8-bit RGBA, opaque on
	//the way in. The background colour is read from the four corners. Pixels in the thin band along the sprite's
	//outline get partial alpha and the sprite's own colour in place of the background's, so there is no white halo.
	void removeFlatBackground(std::uint8_t* rgba, int width, int height, const BackgroundRemovalSettings& settings = {});

	//Gives every fully transparent pixel within `passes` pixels of an opaque one that opaque pixel's colour, so linear
	//filtering and mipmaps never blend a transparent pixel's leftover (white) colour into a sprite's edge.
	void bleedColour(std::uint8_t* rgba, int width, int height, int passes);
}
