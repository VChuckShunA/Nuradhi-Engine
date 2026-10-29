#version 460

//one corner of a camera-facing particle quad, already placed in world space by the CPU
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 colour;
layout(location = 3) in float softness;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColour;
layout(location = 2) out float fragSoftness;

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
	fragColour = colour;
	fragSoftness = softness;
}
