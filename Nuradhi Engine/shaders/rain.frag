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
	vec4 drops; //x size, y length, z spacing, w density
	vec4 motion; //x fall speed, yz wind along x and z, w gusts
	vec4 colour; //rgb lit colour, a opacity
	vec4 lens; //x depth of field, y focus, z mist, w light scatter
	vec4 ripples; //x rate, y size, z life, w strength
	vec4 style; //x splashes, y 1 in realistic mode, z depth scaling, w tail fade
} push;

#include "stylise.glsl"

const float SPLASH_LIFE = 0.22; //seconds a splash crown is in the air

//point lights glinting on whatever water is near them
vec3 glowAt(vec3 position) {
	vec3 glow = vec3(0.0);
	for (int i = 0; i < ubo.numLights; i++) {
		vec3 toLight = ubo.pointLights[i].position.xyz - position;
		glow += ubo.pointLights[i].colour.rgb * ubo.pointLights[i].colour.w / (0.15 + dot(toLight, toLight));
	}
	return glow;
}

//Ripples on the ground. The floor is split into patches; each patch throws a ripple every so often, at a fresh
//spot each time, which spreads, thins and fades. Neighbouring patches are checked so rings can cross patch edges.
vec4 groundRipples() {
	float time = push.centre.w;
	vec2 p = fragWorldPosition.xz;
	float size = max(push.ripples.y, 0.005);
	float patchSize = size * 2.5;
	float rate = max(push.ripples.x * push.drops.w * 2.0, 0.01); //heavier rain, more ripples
	float life = max(push.ripples.z, 0.05);
	vec2 patchCell = floor(p / patchSize);
	//one pixel on the floor, from the smooth position (a distance to a ripple centre jumps at patch edges, and its
	//derivative would draw the patch grid)
	float pixel = length(fwidth(p));

	float rings = 0.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 c = patchCell + vec2(x, y);
			vec3 h = hash3(vec3(c, 91.0));
			float period = (0.6 + 1.6 * h.z) / rate; //uneven, so the floor doesn't pulse in step
			float clock = time + h.x * 17.0 * period;
			float age = mod(clock, period);
			if (age > life) continue;
			vec3 spot = hash3(vec3(c, floor(clock / period))); //a new place for every ripple
			vec2 centre = (c + 0.15 + 0.7 * spot.xy) * patchSize;

			float a = age / life;
			float r = length(p - centre);
			float radius = size * (0.12 + 0.88 * sqrt(a));
			float width = max(0.003 + 0.006 * a, pixel * 0.8);
			float outer = exp(-pow((r - radius) / width, 2.0));
			float inner = exp(-pow((r - radius * 0.55) / width, 2.0)) * 0.45 * smoothstep(0.1, 0.3, a);
			rings += (outer + inner) * (1.0 - a) * (1.0 - a);
		}
	}
	float edges = smoothstep(1.0, 0.8, abs(fragLocal.x)) * smoothstep(1.0, 0.8, abs(fragLocal.y));
	float alpha = clamp(rings, 0.0, 1.0) * push.ripples.w * edges;
	vec3 colour = push.colour.rgb * 1.2 + glowAt(fragWorldPosition) * push.lens.w * 0.4;
	return vec4(colour, alpha);
}

void main() {
	if (fragLayer < 0) {
		vec4 ripples = groundRipples();
		if (ripples.a <= 0.002) discard;
		outColour = ripples;
		return;
	}

	float time = push.centre.w;
	float layer = float(fragLayer);
	vec3 right = normalize(vec3(ubo.invView[0].x, 0.0, ubo.invView[0].z));

	//depth scaling: near layers get longer, faster drops (in an orthographic view that's what sells the depth).
	//At 0 every layer's drops are alike, as the particle rain's are.
	float depthScaling = push.style.z;
	float scale = mix(1.0, mix(0.55, 1.4, fragNearness), depthScaling);
	float pace = mix(1.0, mix(0.8, 1.2, fragNearness), depthScaling);
	float speed = push.motion.x * pace;
	float windAcross = dot(vec3(push.motion.y, 0.0, push.motion.z), right) * pace;
	vec2 direction = normalize(vec2(windAcross, speed)); //of travel, in the layer: across and down (+y)
	vec2 perpendicular = vec2(direction.y, -direction.x);

	float dropLength = push.drops.y * scale;
	float dropSize = push.drops.x * scale;
	vec2 cellSize = vec2(push.drops.z * scale, dropLength * 2.2 + push.drops.z * scale * 2.0);

	//the drop field scrolls along the fall, anchored in the world
	vec2 p = fragPlane;
	vec2 field = vec2(p.x - windAcross * time, p.y - speed * time);
	vec2 cell = floor(field / cellSize);
	float pixel = max(fwidth(p.x), 1e-5); //one pixel, in world units
	float blur = push.lens.x * abs(fragDepthOffset - push.lens.y);

	float drop = 0.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 c = cell + vec2(x, y);
			vec3 h = hash3(vec3(c, layer * 13.0 + 5.0));
			if (h.z > push.drops.w) continue;
			vec2 centre = (c + vec2(0.5 + (h.x - 0.5) * 0.8, h.y)) * cellSize;
			float length_ = dropLength * (0.7 + 0.6 * h.x);

			//a streak: the distance to a line segment along the direction of travel
			vec2 d = field - centre;
			float along = dot(d, direction);
			float across = dot(d, perpendicular);
			float distanceFromStreak = length(vec2(across, max(abs(along) - length_ * 0.5, 0.0)));

			//streaks thinner than a pixel keep their brightness by fading instead of vanishing
			float width = max(dropSize + blur, pixel * 0.7);
			float coverage = (1.0 - smoothstep(width - pixel * 0.5, width + pixel * 0.5, distanceFromStreak)) * clamp(dropSize / width, 0.15, 1.0);
			//tail fade 1: bright at the leading end, fading up the tail. 0: evenly bright with soft ends, the soft
			//ellipse the particle rain draws.
			float tail = smoothstep(-0.5, 0.3, along / length_);
			float ellipse = 1.0 - smoothstep(0.4, 1.0, abs(along) / (length_ * 0.5));
			float taper = mix(ellipse, tail, push.style.w);
			drop = max(drop, coverage * taper * (0.6 + 0.4 * h.y));
		}
	}

	//gusts: the rain sweeps through in sheets, carried by the wind
	float sheetNoise = fbm(vec3(p.x * 0.35 - windAcross * time * 0.6, (p.y - speed * time * 0.25) * 0.25, layer * 0.31), 3);
	float sheet = mix(1.0, smoothstep(0.25, 0.75, sheetNoise) * 1.6, push.motion.w);

	//the sides and the very top fade out so the volume has no edges; far layers sit back in the night
	float edges = smoothstep(1.0, 0.8, abs(fragLocal.x)) * smoothstep(1.0, 0.85, fragLocal.y);
	float opacity = push.colour.a * mix(1.0, mix(0.35, 1.0, fragNearness), depthScaling) * edges;
	float dropAlpha = clamp(drop * opacity * sheet, 0.0, 1.0);

	//realistic: a crown of droplets thrown up where each drop of this layer lands
	float heightAboveGround = push.centre.y - fragWorldPosition.y;
	float splash = 0.0;
	if (push.style.y > 0.5 && push.style.x > 0.0 && heightAboveGround < 0.1) {
		float groundField = push.centre.y - speed * time; //the drop field's y at the ground, right now
		float firstRow = floor(groundField / cellSize.y);
		for (int x = -1; x <= 1; x++) {
			for (int row = -1; row <= 1; row++) {
				vec2 c = vec2(cell.x + float(x), firstRow + float(row));
				vec3 h = hash3(vec3(c, layer * 13.0 + 5.0));
				if (h.z > push.drops.w) continue;
				if (fract(h.y * 7.31 + h.x * 3.17) > 0.6) continue; //not every drop throws a visible crown
				vec2 centre = (c + vec2(0.5 + (h.x - 0.5) * 0.8, h.y)) * cellSize;
				float age = (centre.y - groundField) / speed; //how long ago this drop reached the ground
				if (age < 0.0 || age > SPLASH_LIFE) continue;
				float landedAt = centre.x + windAcross * (time - age);
				float fade = 1.0 - age / SPLASH_LIFE;
				for (int k = 0; k < 4; k++) {
					float angle = (float(k) - 1.5) * 0.55 + (h.x - 0.5) * 0.3;
					float launch = (0.55 + 0.45 * h.y) * scale;
					vec2 droplet = vec2(landedAt + sin(angle) * launch * age, cos(angle) * launch * 1.3 * age - 4.9 * age * age);
					if (droplet.y < 0.0) continue;
					float radius = max(0.002 * scale * (1.0 - 0.5 * age / SPLASH_LIFE), pixel * 0.5);
					float distanceFromDroplet = length(vec2(p.x - droplet.x, heightAboveGround - droplet.y));
					splash = max(splash, (1.0 - smoothstep(radius - pixel * 0.5, radius + pixel * 0.5, distanceFromDroplet)) * fade);
				}
			}
		}
		splash *= push.style.x * edges * mix(0.3, 1.0, fragNearness) * min(push.colour.a * 1.6, 1.0);
	}

	//mist: fine spray hanging low, drifting with the wind, spread over the layers and faded in just above the
	//ground so the layers don't show where they meet the floor
	float drift = fbm(vec3(p.x * 0.8 - windAcross * time * 0.4, p.y * 1.2, layer * 0.37 + time * 0.05), 3);
	float mist = push.lens.z * (0.35 + 0.65 * drift) * exp(-heightAboveGround * 2.5) * edges
		* smoothstep(0.0, 0.15, heightAboveGround) * (1.5 / push.extents.w);

	float waterAlpha = max(dropAlpha, splash);
	float alpha = waterAlpha + mist * (1.0 - waterAlpha);
	if (alpha <= 0.002) discard;

	vec3 glow = glowAt(fragWorldPosition) * push.lens.w * 0.25;
	vec3 water = push.colour.rgb + glow * 1.5; //drops catch the light more than the mist does
	vec3 mistColour = push.colour.rgb * 0.6 + glow * 0.5;
	vec3 colour = (water * waterAlpha + mistColour * mist * (1.0 - waterAlpha)) / alpha;
	outColour = vec4(colour, alpha);
}
