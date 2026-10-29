#version 460
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec2 fragPlane;
layout(location = 2) in vec2 fragLocal;
layout(location = 3) flat in int fragLayer;
layout(location = 4) flat in float fragDepthOffset;
layout(location = 5) flat in float fragNearness;

layout(location = 0) out vec4 outColour;

struct PointLight{
vec4 position; //ignore w
vec4 colour; //w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  vec4 ambientLightColour; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(push_constant) uniform Push {
	vec4 centre; //xyz the ground at the middle of the volume, w time
	vec4 extents; //x half width, y height, z depth, w layer count
	vec4 flakes; //x size, y spacing, z density, w fall speed
	vec4 motion; //x wind, y sway amplitude, z sway frequency, w motion blur
	vec4 colour; //rgb lit colour, a opacity
	vec4 lens; //x depth of field, y focus, z mist, w light scatter
	vec4 style; //x outline, y tone bands, z halftone, w halftone size in pixels
} push;

#include "stylise.glsl"

const float TAU = 6.28318531;

void main() {
	float time = push.centre.w;
	float layer = float(fragLayer);

	//near layers: bigger, faster flakes. In an orthographic view that's what sells the depth.
	float scale = mix(0.45, 1.7, fragNearness);
	float cellSize = push.flakes.y * mix(0.75, 1.4, fragNearness); //far layers aren't much denser, or the distance whites out
	float fallSpeed = push.flakes.w * mix(0.7, 1.35, fragNearness);

	//the flake field scrolls down (+y) and with the wind; it's anchored in the world, so it doesn't slide when
	//the volume follows the camera
	vec2 p = fragPlane;
	vec2 field = vec2(p.x - push.motion.x * time, p.y - fallSpeed * time);
	vec2 cell = floor(field / cellSize);
	float pixel = max(fwidth(field.x), 1e-5); //one pixel, in world units

	//lens: flakes away from the focus depth spread into soft discs, keeping their brightness by area
	float blur = push.lens.x * abs(fragDepthOffset - push.lens.y);
	float stretch = 1.0 + push.motion.w * fallSpeed * 4.0;
	float outlineWidth = push.style.x * fragNearness * fragNearness; //ink on the near flakes; far ones are too small to outline

	float flake = 0.0; //coverage of the nearest flakes, combined
	float ink = 0.0; //coverage of their ink rings
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 c = cell + vec2(x, y);
			vec3 h = hash3(vec3(c, layer * 17.0 + 3.0));
			if (h.z > push.flakes.z) continue;
			vec2 centre = (c + 0.2 + 0.6 * h.xy) * cellSize;
			centre.x += push.motion.y * scale * sin(time * push.motion.z * TAU + h.x * TAU); //flutter
			float radius = push.flakes.x * scale * (0.6 + 0.8 * fract(h.x * 7.13 + h.y * 3.71));

			vec2 d = field - centre;
			d.y /= stretch; //motion blur: drawn out along the fall
			float distanceFromFlake = length(d);

			float soft = max(pixel, blur);
			float r = radius + blur * 0.5;
			float coverage = (1.0 - smoothstep(r - soft, r + soft, distanceFromFlake)) * min(1.0, (radius * radius) / (r * r) * 1.5);
			flake = 1.0 - (1.0 - flake) * (1.0 - coverage);

			if (outlineWidth > 0.0) {
				float ring = r * (1.0 + outlineWidth) + pixel;
				float ringCoverage = 1.0 - smoothstep(ring - pixel, ring + pixel, distanceFromFlake);
				ink = max(ink, ringCoverage);
			}
		}
	}

	//the sides and the very top fade out so the volume has no edges
	float edges = smoothstep(1.0, 0.8, abs(fragLocal.x)) * smoothstep(1.0, 0.85, fragLocal.y);
	//far layers sit further back in the night
	float opacity = push.colour.a * mix(0.45, 1.0, fragNearness) * edges;

	//mist: the snow too far away to see as flakes, thickest near the ground, drifting with the wind. Spread over
	//the layers, and faded in just above the ground so the layers don't show where they meet the floor.
	float heightAboveGround = push.centre.y - fragWorldPosition.y;
	float drift = fbm(vec3(p.x * 0.6 - push.motion.x * time * 0.5, p.y * 0.8, layer * 0.37 + time * 0.03), 3);
	float mistTone = push.lens.z * (0.35 + 0.65 * drift) * exp(-heightAboveGround * 0.7);
	mistTone = posterise(clamp(mistTone, 0.0, 1.0), push.style.y);
	float mist = mistTone * edges * smoothstep(0.0, 0.25, heightAboveGround) * (1.5 / push.extents.w);
	if (push.style.z > 0.0) {
		float dots = halftoneDots(gl_FragCoord.xy, mistTone, push.style.w);
		mist = mix(mist, min(mist * dots * 3.0, 1.0), push.style.z);
	}

	float flakeAlpha = max(flake, ink) * opacity;
	float alpha = flakeAlpha + mist * (1.0 - flakeAlpha);
	if (alpha <= 0.002) discard;

	//light: point lights glow on whatever snow is near them
	vec3 glow = vec3(0.0);
	for (int i = 0; i < ubo.numLights; i++) {
		vec3 toLight = ubo.pointLights[i].position.xyz - fragWorldPosition;
		glow += ubo.pointLights[i].colour.rgb * ubo.pointLights[i].colour.w / (0.15 + dot(toLight, toLight));
	}
	vec3 lit = push.colour.rgb + glow * push.lens.w * 0.25;
	vec3 flakeColour = mix(lit, vec3(0.015), clamp(ink - flake, 0.0, 1.0)); //ink shows only outside the flake
	vec3 mistColour = lit * 0.7;

	vec3 colour = (flakeColour * flakeAlpha + mistColour * mist * (1.0 - flakeAlpha)) / alpha;
	outColour = vec4(colour, alpha);
}
