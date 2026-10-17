#pragma once

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>

namespace nrd {

	enum class TextureFilter {
		Smooth, //linear filtering with mipmaps, for regular textures
		Pixelated, //nearest filtering, top mip only, clamped: crisp pixel art, and frames on a sprite sheet can't bleed into each other
	};

	//Assets are owned by the Renderer; components refer to them by handle
	struct TextureHandle {
		std::uint32_t id = 0; //0 is the renderer's built-in plain white texture
	};

	struct MeshHandle {
		std::uint32_t id = 0xFFFFFFFFu;
		bool valid() const { return id != 0xFFFFFFFFu; }
	};

	//Draws a mesh with the entity's Transform
	struct MeshRenderer {
		MeshHandle mesh{};
		TextureHandle texture{};
		glm::vec4 uvRect{ 0.f, 0.f, 1.f, 1.f }; //part of the texture to show: xy = offset, zw = size (negative width mirrors)
	};

	//A light at the entity's position; it's also drawn as a small glowing billboard
	struct PointLight {
		glm::vec3 colour{ 1.f };
		float intensity = 1.f;
		float radius = 0.1f; //size of the billboard
	};
}
