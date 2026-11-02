#pragma once

#include "engine/ecs/registry.hpp"
#include "engine/physics/collision.hpp"
#include "engine/render/post_process_settings.hpp"
#include "engine/render/render_components.hpp"

//libs
#include <glm/glm.hpp>

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
		//a texture from tightly packed 8-bit RGBA pixels in memory, top row first (e.g. a sprite atlas built at startup)
		TextureHandle loadTextureFromPixels(const std::uint8_t* rgba, int width, int height, TextureFilter filter = TextureFilter::Smooth);
		//Swaps the pixels of a texture that's already loaded, for the same handle: every Sprite and MeshRenderer using it shows
		//the new image from the next frame. Waits for the GPU to finish with the old one first, so it's a stall of a moment.
		void replaceTextureFromPixels(TextureHandle handle, const std::uint8_t* rgba, int width, int height, TextureFilter filter = TextureFilter::Smooth);
		TextureHandle whiteTexture() const { return TextureHandle{ 0 }; }
		MeshHandle loadMesh(const std::string& path); //a .obj file; each path is only loaded once
		MeshHandle spriteQuad() const; //the unit quad every Sprite is drawn on

		//green wireframes of every mesh plus collider boxes and attack hitboxes, drawn over the scene; off by default
		//(the level editor has a checkbox for it, and F1 toggles it)
		void setDebugOverlay(bool enabled);
		bool debugOverlay() const;
		//draws a wire box in the debug overlay for the next frame only, e.g. an attack's hitbox while it's active
		void drawDebugBox(const Aabb& box);
		//draws a wire box for the next frame only even with the debug lines off, e.g. the level editor's selection
		void drawSelectionBox(const Aabb& box);

		//full-screen filters (film noir, CRT) applied to the scene each frame; change them any time
		PostProcessSettings& postProcess();

		float aspectRatio() const;
		//view and projection of the registry's Camera entity for the current window size (Vulkan conventions:
		//+y down in clip space, depth 0..1); identity if there's no camera
		void cameraMatrices(ecs::Registry& registry, glm::mat4& view, glm::mat4& projection);

		//Dear ImGui: the engine starts a UI frame every frame, so game code can call ImGui:: functions from any
		//system; the UI is drawn on top of everything else
		void beginUiFrame();
		bool uiWantsKeyboard() const; //an ImGui text field has focus
		bool uiWantsMouse() const; //the mouse is over an ImGui window

		void render(ecs::Registry& registry, float frameTime);
		void waitIdle(); //blocks until the GPU is done, before anything it uses is destroyed

	private:
		struct Impl;
		std::unique_ptr<Impl> impl;
	};
}
