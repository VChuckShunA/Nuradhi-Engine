#version 460

layout (location = 0) in vec3 fragColour;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fragUV;

layout (location = 0) out vec4 outColour;

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


void main(){
	vec3 diffuseLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;
	vec3 specularLight = vec3(0.0);
	vec3 surfaceNormal = normalize(fragNormalWorld);

	vec3 cameraPosWorld=ubo.invView[3].xyz;
	vec3 viewDirection=normalize(cameraPosWorld - fragPosWorld);

	for(int i =0; i <ubo.numLights; i++){
	PointLight light =  ubo.pointLights[i];	
	vec3 directionToLight= light.position.xyz - fragPosWorld;
	float attenuation=1.0/dot(directionToLight,directionToLight); //distance squared
	directionToLight=normalize(directionToLight);

	float cosAngIncidence = max(dot(surfaceNormal,directionToLight ),0);
	vec3 intensity = light.colour.xyz * light.colour.w *attenuation;
	
	diffuseLight+= intensity * cosAngIncidence;

	//specular lighting
	vec3 halfAngle = normalize(directionToLight + viewDirection);
	float blinnTerm  = dot(surfaceNormal,halfAngle);
	blinnTerm = clamp(blinnTerm,0,1);
	blinnTerm = pow(blinnTerm,32.0); //high values -> sharper highlight
	specularLight= intensity*blinnTerm;
	}
	
	vec3 imageColour = texture(image, fragUV).rgb;
  
	outColour = vec4((diffuseLight * imageColour + specularLight * imageColour), 1.0);
	//outColour = vec4(diffuseLight * fragColour +specularLight*fragColour,1.0);
}