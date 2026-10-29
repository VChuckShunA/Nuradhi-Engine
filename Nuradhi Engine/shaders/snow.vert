#version 460

//One layer of a snow volume: an upright quad facing the camera, standing on the ground. No vertex buffer: each
//instance is a layer, each vertex a corner of it. Instance 0 is the farthest layer, so the layers draw back to front.

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec2 fragPlane; //x across the layer (world anchored), y up from the ground
layout(location = 2) out vec2 fragLocal; //x -1..1 across the layer, y 0..1 from the ground to the top
layout(location = 3) flat out int fragLayer;
layout(location = 4) flat out float fragDepthOffset; //how far the layer is behind (+) or in front of (-) the centre
layout(location = 5) flat out float fragNearness; //0 for the farthest layer, 1 for the nearest

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

const vec2 CORNERS[6] = vec2[](
	vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
	vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(-1.0, 0.0)
);

void main() {
	float layers = push.extents.w;
	float nearness = (float(gl_InstanceIndex) + 0.5) / layers;
	float depthOffset = push.extents.z * (1.0 - 2.0 * nearness);

	//the layers stand upright and face the camera's heading, so turning the camera turns them with it
	vec3 forward = normalize(vec3(ubo.invView[2].x, 0.0, ubo.invView[2].z));
	vec3 right = normalize(vec3(ubo.invView[0].x, 0.0, ubo.invView[0].z));

	vec2 corner = CORNERS[gl_VertexIndex];
	vec3 position = push.centre.xyz + forward * depthOffset
		+ right * (corner.x * push.extents.x)
		+ vec3(0.0, -corner.y * push.extents.y, 0.0); //up is -y

	gl_Position = ubo.projection * ubo.view * vec4(position, 1.0);
	fragWorldPosition = position;
	fragPlane = vec2(dot(position, right), position.y);
	fragLocal = corner;
	fragLayer = gl_InstanceIndex;
	fragDepthOffset = depthOffset;
	fragNearness = nearness;
}
