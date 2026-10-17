#pragma once

#include "engine/ecs/registry.hpp"
#include "engine/render/render_components.hpp"

//std
#include <cstdint>
#include <memory>
#include <string>

namespace lve {
	class LveWindow;
}

namespace nrd {

	//The rendering backend. It owns the Vulkan device, swapchain, pipelines and every loaded texture and mesh.
	//Game code loads assets here and puts the handles in components (MeshRenderer, Sprite); the engine calls
	//render() each frame to draw the registry from its Camera entity.
	class Renderer {
	public:
		static constexpr std::uint32_t MAX_TEXTURES = 16; //sizes the descriptor pool: each texture has one global descriptor set per frame

		explicit Renderer(lve::LveWindow& window);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		TextureHandle loadTexture(const std::string& path, TextureFilter filter = TextureFilter::Smooth);
		TextureHandle whiteTexture() const { return TextureHandle{ 0 }; }
		MeshHandle loadMesh(const std::string& path); //a .obj file; each path is only loaded once
		MeshHandle spriteQuad() const; //the unit quad every Sprite is drawn on

		//green wireframes of every mesh plus collider boxes, drawn over the scene
		void setDebugOverlay(bool enabled);
		bool debugOverlay() const;

		float aspectRatio() const;

		void render(ecs::Registry& registry, float frameTime);
		void waitIdle(); //blocks until the GPU is done, before anything it uses is destroyed

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
}
