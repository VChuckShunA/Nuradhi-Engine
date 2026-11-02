#version 460

//one corner of a blood droplet (a camera-facing quad, stretched along its motion) or splat (a quad lying flat on
//the floor), already placed in world space by the CPU

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 data; //x 0 for a droplet, 1 for a splat; y opacity; z a random seed for the splat's shape

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec2 fragUV;
layout(location = 2) flat out vec4 fragData;

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
	fragWorldPosition = position;
	fragUV = uv;
	fragData = data;
}
