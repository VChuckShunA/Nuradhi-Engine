#version 460

layout (location = 0) in vec3 fragColour;
layout (location = 0) out vec4 outColour;

layout(push_constant) uniform Push{
	mat4 transform; //projection * view * model- used transform model position
	//mat4 modelMatrix;
	mat4 normalMatrix;
} push;

void main(){
	outColour = vec4(fragColour,1.0);
}