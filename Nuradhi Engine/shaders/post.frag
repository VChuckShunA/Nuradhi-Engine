#version 460

//The full-screen filters: the CRT bends the picture and splits its colours as it samples the scene, the film
//noir grade turns it into contrasty black and white film, and the CRT then lays its scanlines and phosphor
//stripes over the result. The scene is sampled from an sRGB image, so it arrives linear; grading happens in
//gamma space, where contrast and grain behave the way they do on film, and the result goes back out linear.

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColour;

layout(set = 0, binding = 0) uniform sampler2D scene;

layout(push_constant) uniform Push {
	vec4 noir; //x enabled, y contrast, z brightness, w grain
	vec4 noirLook; //x vignette, y keep red, z time, w keep blue
	vec4 crt; //x enabled, y curvature, z scanlines, w mask
	vec4 crtLook; //x aberration (pixels), y flicker, z vignette, w scanline size (pixels)
	vec4 screen; //xy size in pixels
	vec4 flash; //rgb lightning light added to the picture (gamma space), w its brightness
} push;

float hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void main() {
	bool crtOn = push.crt.x > 0.5;
	bool noirOn = push.noir.x > 0.5;
	float time = push.noirLook.z;
	vec2 uv = fragUV;

	//CRT: the glass bulges, so the picture curves away towards the corners
	vec2 centred = uv * 2.0 - 1.0;
	if (crtOn) {
		centred *= 1.0 + push.crt.y * dot(centred, centred) * 0.5;
		uv = centred * 0.5 + 0.5;
	}

	//sample, with the CRT's colour fringing growing towards the edges
	vec3 colour;
	if (crtOn && push.crtLook.x > 0.0) {
		vec2 fringe = centred * push.crtLook.x / push.screen.xy;
		colour = vec3(texture(scene, uv + fringe).r, texture(scene, uv).g, texture(scene, uv - fringe).b);
	}
	else {
		colour = texture(scene, uv).rgb;
	}

	vec3 graded = pow(max(colour, vec3(0.0)), vec3(1.0 / 2.2));

	//lightning: the whole picture is lit for an instant. The dark sky and shadows lift towards the flash colour, and
	//the exposure jumps, so the film grade below blows highlights out the way a real flash does
	graded = graded * (1.0 + push.flash.w * 0.25) + push.flash.rgb;

	//film noir: panchromatic black and white, a hard S-curve, grain and a vignette
	if (noirOn) {
		float grey = dot(graded, vec3(0.36, 0.53, 0.11)); //film sees reds a little brighter than the eye does
		float toned = (grey + push.noir.z - 0.5) * push.noir.y + 0.5;
		toned = clamp(toned, 0.0, 1.0);
		toned = mix(toned, toned * toned * (3.0 - 2.0 * toned), 0.4); //deep blacks, bright highlights
		//keep red and keep blue: strong reds or blues survive the greyscale, pushed through the same contrast
		vec3 contrasted = clamp((graded + push.noir.z - 0.5) * push.noir.y + 0.5, 0.0, 1.0);
		float redness = clamp((graded.r - max(graded.g, graded.b)) * 3.0 - 0.2, 0.0, 1.0) * push.noirLook.y;
		float blueness = clamp((graded.b - max(graded.r, graded.g)) * 3.0 - 0.2, 0.0, 1.0) * push.noirLook.w;
		graded = mix(vec3(toned), contrasted, max(redness, blueness));

		//grain moves every frame (24 times a second, like film) and shows most in the mid tones
		float frame = floor(time * 24.0);
		float grain = hash(gl_FragCoord.xy + frame * 37.0) - 0.5;
		float midtones = 1.0 - abs(dot(graded, vec3(0.333)) * 2.0 - 1.0) * 0.6;
		graded += grain * push.noir.w * midtones;

		float distanceFromCentre = length(fragUV - 0.5) * 1.414; //0 at the centre, 1 in the corners
		graded *= 1.0 - push.noirLook.x * smoothstep(0.35, 1.05, distanceFromCentre);
	}

	if (crtOn) {
		//scanlines: dark gaps between lines, softer where the picture is bright (bright lines bloom wider)
		float brightness = dot(graded, vec3(0.333));
		float line = 0.5 + 0.5 * cos(uv.y * push.screen.y * 6.28318531 / push.crtLook.w);
		graded *= mix(1.0, mix(1.0 - push.crt.z, 1.0, line), 1.0 - brightness * 0.4);

		//aperture grille: red, green and blue phosphor stripes, brightened to make up for what they block
		int column = int(gl_FragCoord.x) % 3;
		vec3 stripe = column == 0 ? vec3(1.0, 0.35, 0.35) : column == 1 ? vec3(0.35, 1.0, 0.35) : vec3(0.35, 0.35, 1.0);
		graded *= mix(vec3(1.0), stripe * 1.5, push.crt.w);

		//flicker, and the tube's dark, rounded edges
		graded *= 1.0 + push.crtLook.y * sin(time * 110.0);
		vec2 edge = abs(centred);
		graded *= 1.0 - push.crtLook.z * smoothstep(0.55, 1.0, max(edge.x, edge.y));
		vec2 corner = max(abs(uv - 0.5) * 2.0 - 0.94, 0.0); //rounded corners: outside a 0.06 radius is off the glass
		graded *= 1.0 - smoothstep(0.05, 0.06, length(corner));
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) graded = vec3(0.0); //off the curved glass
	}

	outColour = vec4(pow(clamp(graded, 0.0, 1.0), vec3(2.2)), 1.0);
}
