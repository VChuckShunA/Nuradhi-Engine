#version 460

//One horizontal slice of a fog volume. No vertex buffer: each instance is a slice, each vertex a corner of it.
//Instance 0 is the slice nearest the ground. The camera looks down on the fog, so that slice is the farthest away
//and the instances draw back to front.

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out float fragHeight; //0 at the ground, 1 at the top of the fog

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

const vec2 CORNERS[6] = vec2[](
	vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
	vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0)
);

void main() {
	float height = (float(gl_InstanceIndex) + 0.5) / push.extents.w;
	vec2 corner = CORNERS[gl_VertexIndex];
	vec3 position = vec3(
		push.centre.x + corner.x * push.extents.x,
		push.centre.y - height * push.extents.z, //up is -y
		push.centre.z + corner.y * push.extents.y);

	gl_Position = ubo.projection * ubo.view * vec4(position, 1.0);
	fragWorldPosition = position;
	fragHeight = height;
}
