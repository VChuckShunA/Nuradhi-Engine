#version 460

layout (location = 0) in vec3 fragColour;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;

layout (location = 0) out vec4 outColour;

struct PointLight{
vec4 position; //ignore w
vec4 colour; //w is intensity
};


layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  vec4 ambientLightColour; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(push_constant) uniform Push{
	mat4 modelMatrix; //projection * view * model- used transform model position
	mat4 normalMatrix;
} push;


void main(){
	vec3 diffuseLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;
	vec3 surfaceNormal = normalize(fragNormalWorld);

	for(int i =0; i <ubo.numLights; i++){
	PointLight light =  ubo.pointLights[i];	
	vec3 directionToLight= light.position.xyz - fragPosWorld;
	float attenuation=1.0/dot(directionToLight,directionToLight); //distance squared
	float cosAngIncidence = max(dot(surfaceNormal, normalize(directionToLight)),0);
	vec3 intensity = light.colour.xyz * light.colour.w *attenuation;
	
	diffuseLight+= intensity * cosAngIncidence;
	
	}

	outColour = vec4(diffuseLight * fragColour,1.0);
}