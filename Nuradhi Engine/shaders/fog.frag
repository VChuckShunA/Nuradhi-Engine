#version 460
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in float fragHeight;

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
	vec4 extents; //x half width, y half depth, z height, w slice count
	vec4 colour; //rgb lit colour, a density
	vec4 shape; //x coverage, y height falloff, z noise scale, w turbulence
	vec4 motion; //xy wind along x and z, z shadow, w light scatter
	vec4 style; //x tone bands, y halftone, z halftone size in pixels
} push;

#include "stylise.glsl"

void main() {
	float time = push.centre.w;
	vec3 p = fragWorldPosition;

	//the noise field lives in world space and drifts with the wind, so it stays put as the volume follows the camera
	vec3 q = vec3(p.x - push.motion.x * time, p.y * 2.5, p.z - push.motion.y * time) * push.shape.z;
	q.y += time * 0.05; //slow churn, so the billows change shape as well as move
	//domain warp: offsetting the lookup by more noise curls the billows into wisps
	vec3 warp = vec3(fbm(q + vec3(0.0, 0.0, time * 0.07), 3), fbm(q + vec3(5.2, 1.3, -time * 0.05), 3), 0.0);
	float n = fbm(q + push.shape.w * 1.8 * (warp - 0.5), 5);

	//thick near the ground and thinning with height; the top and sides fade out so the volume has no edges
	float coverage = push.shape.x;
	float billow = smoothstep(coverage * 0.6, 0.6 + coverage * 0.4, n);
	float profile = exp(-fragHeight * push.shape.y) * smoothstep(1.0, 0.7, fragHeight);
	vec2 edge = abs(p.xz - push.centre.xz) / push.extents.xy;
	profile *= smoothstep(1.0, 0.75, edge.x) * smoothstep(1.0, 0.6, edge.y);
	float tone = billow * profile;

	//graphic novel: posterise into hard-edged tones (anti-aliased across one pixel)
	tone = posterise(tone, push.style.x);

	//each slice holds its share of the slab. A view ray crosses every slice at a slant, so it goes through more fog
	//than the slice's thickness: scale by the slant (looking straight down would be 1)
	vec3 forward = normalize(ubo.invView[2].xyz);
	float slant = 1.0 / max(abs(forward.y), 0.2);
	float thickness = push.extents.z / push.extents.w;
	float alpha = 1.0 - exp(-push.colour.a * tone * thickness * slant);

	//halftone: a 45-degree dot screen in screen space. Every slice uses the same grid, so the dots stack up and grow
	//with the fog's density, like a printed tone
	if (push.style.y > 0.0) {
		float dots = halftoneDots(gl_FragCoord.xy, tone, push.style.z);
		alpha = mix(alpha, min(alpha * dots * 2.2, 1.0), push.style.y);
	}
	if (alpha <= 0.001) discard;

	//light: thin fog sits in its own shade, thick billows catch the light, and point lights glow through it
	vec3 colour = push.colour.rgb * mix(1.0 - push.motion.z, 1.0, smoothstep(0.0, 0.8, billow));
	vec3 glow = vec3(0.0);
	for (int i = 0; i < ubo.numLights; i++) {
		vec3 toLight = ubo.pointLights[i].position.xyz - p;
		glow += ubo.pointLights[i].colour.rgb * ubo.pointLights[i].colour.w / (0.15 + dot(toLight, toLight));
	}
	colour += glow * push.motion.w * 0.25;

	outColour = vec4(colour, alpha);
}
