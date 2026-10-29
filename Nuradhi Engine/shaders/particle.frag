#version 460

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColour;
layout(location = 2) in float fragSoftness;

layout(location = 0) out vec4 outColour;

void main() {
	//a disc filling the quad (an ellipse when the quad is stretched into a rain streak). softness is how much of
	//the radius fades out towards the edge: near 0 is a crisp dot, 1 a gradient from the centre, like fog.
	float distanceFromCentre = length(fragUV * 2.0 - 1.0);
	float edge = max(fragSoftness, 0.02);
	float alpha = fragColour.a * (1.0 - smoothstep(1.0 - edge, 1.0, distanceFromCentre));
	if (alpha <= 0.002) discard;
	outColour = vec4(fragColour.rgb, alpha);
}
