#version 460

layout(location = 0) in vec2 fragUV;
layout(location = 1) flat in vec4 fragData;

layout(location = 0) out vec4 outColour;

layout(push_constant) uniform Push {
	vec4 colour; //rgb the glow around the bolt, linear
} push;

void main() {
	float d = abs(fragUV.x * 2.0 - 1.0); //0 along the middle of the ribbon, 1 at its edge
	float coreEdge = max(fragData.x, 0.01);
	float r = d / coreEdge; //1 at the edge of the white-hot core

	float core = 1.0 - smoothstep(0.55, 1.0, r);
	float halo = 1.0 / (1.0 + r * r * 0.9); //falls off like light does, a long soft tail
	halo *= 1.0 - smoothstep(0.55, 1.0, d); //and is gone by the edge of the ribbon, so it never shows a box

	float alpha = clamp(core + halo * 0.6, 0.0, 1.0) * fragData.y;
	if (alpha <= 0.002) discard;
	vec3 colour = mix(push.colour.rgb * 1.6, vec3(1.0), core);
	outColour = vec4(colour, alpha);
}
