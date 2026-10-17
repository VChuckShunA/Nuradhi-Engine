#version 460
layout(location = 0) out vec4 outColour;  // Output colour

const vec4 wireframeColour=vec4(0.0,1.0,0.0,0.5); // Colour of the wireframe
const float wireframeThickness=5.0; // Thickness of the lines

void main() {
  outColour = wireframeColour;
}