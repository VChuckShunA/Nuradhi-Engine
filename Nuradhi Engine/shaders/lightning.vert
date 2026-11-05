#version 460

//one corner of a piece of lightning bolt (a ribbon along a segment of the bolt), already placed in world space by the CPU

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv; //x across the ribbon, 0 to 1; y along it: 0 in the body, -1 / +1 at the tip of a rounded start / end
layout(location = 2) in vec4 data; //x the share of the ribbon's half width that is the white-hot core; y brightness

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragData; //smooth: brightness changes along the bolt

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

void main() {
	gl_Position = ubo.projection * ubo.view * vec4(position, 1.0);
	fragUV = uv;
	fragData = data;
}
