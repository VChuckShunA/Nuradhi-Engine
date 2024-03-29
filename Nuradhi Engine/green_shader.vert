#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 colour;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;


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


layout(push_constant) uniform Push{
	mat4 modelMatrix; //projection * view * model- used transform model position
	mat4 normalMatrix;
} push;




void main() {
    
    vec4 positionWorld = push.modelMatrix*vec4(position,1.0);
	gl_Position=ubo.projection* ubo.view * positionWorld;

	
}