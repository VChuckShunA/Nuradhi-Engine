#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 colour;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 uv;

layout(location = 0) out vec3 fragColour;

layout(set=0,binding=0) uniform GlobalUbo{
	mat4 projectionViewMatrix;
	vec3 directionToLight;
} ubo;

layout(push_constant) uniform Push{
	mat4 modelMatrix; //projection * view * model- used transform model position
	mat4 normalMatrix;
} push;


const float AMBIENT = 0.02; //consider changing this for noir effects


void main(){
	gl_Position=ubo.projectionViewMatrix* push.modelMatrix*vec4(position,1.0);

	//temporary: this is only correct in certain situations!
	//only works correctly if sclae is uniform(sx==sy==sz)
	//vec3 normalWorldSpace = normalize(mat3(push.modelMatrix)*normal);
	//alternative normalWorldSpace = normalize((push.modelMatrix*vec4(normal,0.0).xyz)
	
	//calculating the inverse in a shader can be expensive and should be avoided
	//mat3 normalMatrix=transpose(inverse(mat3(push.modelMatrix)));
	//vec3 normalWorldSpace=normalize(normalMatrix*normal);

	vec3 normalWorldSpace=normalize(mat3(push.normalMatrix)*normal);

	float lightIntensity=AMBIENT+max(dot(normalWorldSpace, ubo.directionToLight),0);
	
	fragColour=lightIntensity * colour;
}