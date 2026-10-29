#version 460

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec2 fragUV;
layout(location = 2) flat in float fragKind;
layout(location = 3) in float fragAge;

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
	vec4 colour; //rgb water, w light glint
	vec4 strength; //x ripples, y splashes
} push;

void main() {
	float r = length(fragUV * 2.0 - 1.0); //0 at the centre, 1 at the quad's edge
	float alpha;
	if (fragKind < 0.5) {
		//a ripple: a ring that spreads to the quad's edge, thinning and fading, with a fainter ring inside it
		float a = fragAge;
		float radius = 0.12 + 0.88 * sqrt(a);
		float width = max(0.04 + 0.08 * a, fwidth(r) * 1.2);
		float outer = exp(-pow((r - radius) / width, 2.0));
		float inner = exp(-pow((r - radius * 0.55) / width, 2.0)) * 0.45 * smoothstep(0.1, 0.3, a);
		alpha = clamp(outer + inner, 0.0, 1.0) * (1.0 - a) * (1.0 - a) * push.strength.x;
	}
	else {
		//a splash droplet: a small soft bead
		alpha = (1.0 - smoothstep(0.5, 1.0, r)) * (1.0 - fragAge) * push.strength.y;
	}
	if (alpha <= 0.002) discard;

	//water glints where point lights are near
	vec3 glow = vec3(0.0);
	for (int i = 0; i < ubo.numLights; i++) {
		vec3 toLight = ubo.pointLights[i].position.xyz - fragWorldPosition;
		glow += ubo.pointLights[i].colour.rgb * ubo.pointLights[i].colour.w / (0.15 + dot(toLight, toLight));
	}
	outColour = vec4(push.colour.rgb * 1.2 + glow * push.colour.w * 0.4, alpha);
}
