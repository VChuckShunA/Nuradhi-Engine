#version 460

//One piece of a rain volume, with no vertex buffer: each instance is a quad, each vertex a corner of it.
//Instance 0 is the ground the rain lands on (it carries the ripples, and is only drawn in realistic mode); the
//rest are upright layers facing the camera, standing on the ground, from the farthest (1) to the nearest.

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec2 fragPlane; //layers: x across the layer (world anchored), y the world height
layout(location = 2) out vec2 fragLocal; //-1..1 across; layers 0..1 from the ground up, the ground -1..1 in depth
layout(location = 3) flat out int fragLayer; //-1 for the ground
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
	vec4 drops; //x size, y length, z spacing, w density
	vec4 motion; //x fall speed, yz wind along x and z, w gusts
	vec4 colour; //rgb lit colour, a opacity
	vec4 lens; //x depth of field, y focus, z mist, w light scatter
	vec4 ripples; //x rate, y size, z life, w strength
	vec4 style; //x splashes, y 1 in realistic mode, z depth scaling, w tail fade
} push;

const vec2 CORNERS[6] = vec2[](
	vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
	vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(-1.0, 0.0)
);

void main() {
	//the layers stand upright and face the camera's heading, so turning the camera turns them with it
	vec3 forward = normalize(vec3(ubo.invView[2].x, 0.0, ubo.invView[2].z));
	vec3 right = normalize(vec3(ubo.invView[0].x, 0.0, ubo.invView[0].z));
	vec2 corner = CORNERS[gl_VertexIndex];
	vec3 position;

	if (gl_InstanceIndex == 0) {
		//the ground: a flat quad a hair above the floor, as wide and deep as the layers
		vec2 local = vec2(corner.x, corner.y * 2.0 - 1.0);
		position = push.centre.xyz + right * (local.x * push.extents.x) + forward * (local.y * push.extents.z)
			+ vec3(0.0, -0.003, 0.0);
		fragPlane = vec2(0.0);
		fragLocal = local;
		fragLayer = -1;
		fragDepthOffset = 0.0;
		fragNearness = 0.0;
	}
	else {
		int layer = gl_InstanceIndex - 1;
		float nearness = (float(layer) + 0.5) / push.extents.w;
		float depthOffset = push.extents.z * (1.0 - 2.0 * nearness);
		position = push.centre.xyz + forward * depthOffset
			+ right * (corner.x * push.extents.x)
			+ vec3(0.0, -corner.y * push.extents.y, 0.0); //up is -y
		fragPlane = vec2(dot(position, right), position.y);
		fragLocal = corner;
		fragLayer = layer;
		fragDepthOffset = depthOffset;
		fragNearness = nearness;
	}

	gl_Position = ubo.projection * ubo.view * vec4(position, 1.0);
	fragWorldPosition = position;
}
