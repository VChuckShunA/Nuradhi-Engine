#version 460

layout (location = 0) in vec3 fragColour;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;

layout (location = 0) out vec4 outColour;

layout(set=0,binding=0) uniform GlobalUbo{
	mat4 projection;
	mat4 view;
	vec4 ambientLightColour; //w is intensity
	vec3 lightPosition;
	vec4 lightColour;
} ubo;

layout(push_constant) uniform Push{
	mat4 modelMatrix; //projection * view * model- used transform model position
	mat4 normalMatrix;
} push;


void main(){
	
	vec3 directionToLight= ubo.lightPosition -fragPosWorld;
	float attenuation=1.0/dot(directionToLight,directionToLight); //distance squared

	vec3 lightColour = ubo.lightColour.xyz * ubo.lightColour.w * attenuation;
	vec3 ambientLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;
	vec3 diffuseLight = lightColour * max(dot(normalize(fragNormalWorld), normalize(directionToLight)),0);
	

	outColour = vec4((diffuseLight+ambientLight) * fragColour,1.0);
}