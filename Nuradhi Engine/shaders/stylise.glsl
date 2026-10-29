//Shared by the volumetric weather shaders: noise for the billows and mist, and the graphic novel treatments.
//Pulled in with #include (glslc's GL_GOOGLE_include_directive).

//value noise from a cheap, stable hash (Dave Hoskins' hash13 and hash33)
float hash(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

vec3 hash3(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

float noise(vec3 p) {
	vec3 cell = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);
	return mix(
		mix(mix(hash(cell + vec3(0, 0, 0)), hash(cell + vec3(1, 0, 0)), u.x),
			mix(hash(cell + vec3(0, 1, 0)), hash(cell + vec3(1, 1, 0)), u.x), u.y),
		mix(mix(hash(cell + vec3(0, 0, 1)), hash(cell + vec3(1, 0, 1)), u.x),
			mix(hash(cell + vec3(0, 1, 1)), hash(cell + vec3(1, 1, 1)), u.x), u.y), u.z);
}

//fractal noise: each octave half the size and half the strength, rotated so the grids don't line up
float fbm(vec3 p, int octaves) {
	const mat3 ROTATE = mat3(0.00, 0.80, 0.60, -0.80, 0.36, -0.48, -0.60, -0.48, 0.64);
	float sum = 0.0;
	float amplitude = 0.5;
	float total = 0.0;
	for (int i = 0; i < octaves; i++) {
		sum += amplitude * noise(p);
		total += amplitude;
		p = ROTATE * p * 2.03;
		amplitude *= 0.5;
	}
	return sum / total;
}

//posterises a 0..1 tone into hard-edged bands, anti-aliased across one pixel. 0 bands leaves it smooth.
float posterise(float tone, float bands) {
	if (bands <= 0.0) return tone;
	float level = tone * bands;
	float pixel = fwidth(level);
	return (floor(level) + smoothstep(1.0 - pixel, 1.0, fract(level))) / bands;
}

//a 45-degree halftone dot screen in screen space: 1 inside a dot, 0 outside. The dots grow with the tone, and
//every layer drawn with the same spacing shares the grid, so stacked layers read as one printed tone.
float halftoneDots(vec2 fragCoord, float tone, float spacing) {
	const float c = 0.70710678;
	vec2 grid = mat2(c, -c, c, c) * fragCoord / spacing;
	float distanceFromDot = length(fract(grid) - 0.5);
	float radius = 0.72 * sqrt(clamp(tone, 0.0, 1.0));
	float aa = fwidth(distanceFromDot);
	return 1.0 - smoothstep(radius - aa, radius + aa, distanceFromDot);
}
