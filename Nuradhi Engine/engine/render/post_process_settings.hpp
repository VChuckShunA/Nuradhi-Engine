#pragma once

namespace nrd {

	//Black and white film: a panchromatic greyscale with a hard contrast curve, crushed blacks, moving film grain
	//and a vignette. keepRed and keepBlue let strong reds or blues survive the greyscale, the way a graphic novel picks
	//out one colour.
	struct FilmNoirFilter {
		bool enabled = false;
		float contrast = 1.25f; //1 leaves the tones alone; higher pushes towards hard blacks and whites
		float brightness = 0.08f; //-0.5..0.5, applied before the contrast (a little lift keeps dark sprites readable)
		float grain = 0.07f; //0..0.3 strength of the moving film grain
		float vignette = 0.55f; //0..1 how dark the corners fall
		float keepRed = 0.f; //0..1: strong reds (blood, Blaze's outfit) keep their colour through the greyscale
		float keepBlue = 0.f; //0..1: strong blues keep their colour, the same way
	};

	//An old cathode ray tube: a curved screen with rounded, dark edges, scanlines, an RGB aperture grille, colour
	//fringing towards the edges and a faint flicker.
	struct CrtFilter {
		bool enabled = false;
		float curvature = 0.1f; //0..0.3 how much the glass bulges
		float scanlines = 0.45f; //0..1 how dark the gaps between lines are
		float scanlineSize = 3.f; //pixels from one scanline to the next
		float mask = 0.3f; //0..1 strength of the red, green and blue phosphor stripes
		float aberration = 1.5f; //pixels the colours fringe apart at the edges
		float flicker = 0.03f; //0..0.2 brightness flicker
		float vignette = 0.35f; //0..1 how dark the tube's edges fall
	};

	//Full-screen filters applied to the finished scene (not to the editor UI). With both off the scene is drawn
	//straight to the screen and costs nothing extra.
	struct PostProcessSettings {
		FilmNoirFilter noir;
		CrtFilter crt;
	};
}
