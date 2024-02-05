#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 colour;

layout(location = 0) out vec3 fragColour;

layout(push_constant) uniform Push{
	mat4 transform;
	vec3 colour;
} push;

void main(){
	gl_Position=push.transform*vec4(position,1.0);
	fragColour=colour;
}