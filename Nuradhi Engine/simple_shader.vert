#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 colour;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColour;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragUV;

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

layout(set = 0, binding = 1) uniform sampler2D image;

layout(push_constant) uniform Push{
	mat4 modelMatrix; //projection * view * model- used transform model position
	mat4 normalMatrix;
} push;


//consider changing AMBIENT VALUE for noir effects


void main(){
	vec4 positionWorld = push.modelMatrix*vec4(position,1.0);
	gl_Position=ubo.projection* ubo.view * positionWorld;
	fragNormalWorld=normalize(mat3(push.normalMatrix)*normal);
	fragPosWorld=positionWorld.xyz;
	fragColour=colour;
	fragUV=uv;
}