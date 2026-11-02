#version 460

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec2 fragUV;
layout(location = 2) flat in vec4 fragData;

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
	vec4 colour; //rgb blood, linear
} push;

void main() {
	vec2 p = fragUV * 2.0 - 1.0;
	float r = length(p);
	float aa = fwidth(r);
	float coverage;
	vec3 colour = push.colour.rgb;

	if (fragData.x < 0.5) {
		//a droplet: a round bead (the quad is stretched along its motion, so it reads as a streak in flight)
		coverage = 1.0 - smoothstep(0.75 - aa, 0.75 + aa, r);
	}
	else {
		//a splat: a blob whose edge wobbles with a few lobes, plus a couple of flecks thrown off it
		float seed = fragData.z;
		float angle = atan(p.y, p.x);
		float edge = 0.55 + 0.08 * sin(angle * 3.0 + seed) + 0.06 * sin(angle * 5.0 + seed * 1.7) + 0.04 * sin(angle * 9.0 + seed * 2.3);
		float soft = aa + fragData.w * 0.3; //rain-washed blood bleeds into the wet floor: a softer edge
		coverage = 1.0 - smoothstep(edge - soft, edge + soft, r);
		for (int i = 0; i < 3; i++) {
			float a = seed * (1.3 + float(i)) + float(i) * 2.1;
			vec2 fleck = vec2(cos(a), sin(a)) * (0.72 + 0.08 * float(i));
			float fleckSize = 0.09 - 0.02 * float(i);
			coverage = max(coverage, (1.0 - smoothstep(fleckSize - aa, fleckSize + aa, length(p - fleck))) * (1.0 - fragData.w)); //the flecks wash away first
		}
		colour *= 0.75 + 0.25 * smoothstep(edge, 0.0, r); //pooled darker at the rim
		colour = mix(colour, colour + vec3(0.3, 0.16, 0.16), fragData.w * 0.7); //diluted by the rain: paler, pinker
	}
	float alpha = coverage * fragData.y;
	if (alpha <= 0.002) discard;

	//a wet sheen where point lights are near; kept red so the noir filter's keep red still finds it
	vec3 glow = vec3(0.0);
	for (int i = 0; i < ubo.numLights; i++) {
		vec3 toLight = ubo.pointLights[i].position.xyz - fragWorldPosition;
		glow += ubo.pointLights[i].colour.rgb * ubo.pointLights[i].colour.w / (0.15 + dot(toLight, toLight));
	}
	colour *= 1.0 + dot(glow, vec3(0.333)) * 1.5;
	outColour = vec4(colour, alpha);
}
