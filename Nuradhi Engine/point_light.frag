#version 460

layout (location = 0) in vec2 fragOffset;
layout (location = 0) out vec4 outColour;

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  vec4 ambientLightColour; // w is intensity
  vec3 lightPosition;
  vec4 lightColour;
} ubo;

void main() {
  float dis = sqrt(dot(fragOffset, fragOffset));
  if (dis >= 1.0) {
    discard;
  }
  outColour = vec4(ubo.lightColour.xyz, 1.0);
}