#pragma once

#include "engine/render/vulkan/lve_buffer.hpp"
#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_pipeline.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <memory>
#include <vector>

namespace nrd {

	//Draws every ParticleEmitter's particles as camera-facing quads (stretched along their velocity when the
	//emitter asks for it), alpha blended back to front without writing depth. The quads are built on the CPU each
	//frame into a per-frame vertex buffer.
	class ParticleRenderSystem {
	public:
		static constexpr std::uint32_t MAX_PARTICLES = 8000; //drawn per frame across all emitters; the farthest are dropped first

		ParticleRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~ParticleRenderSystem();

		ParticleRenderSystem(const ParticleRenderSystem&) = delete;
		ParticleRenderSystem& operator=(const ParticleRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

		struct Vertex {
			glm::vec3 position;
			glm::vec2 uv;
			glm::vec4 colour;
			float softness;
		};

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);

		lve::LveDevice& lveDevice;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<lve::LvePipeline> pipeline;
		std::vector<std::unique_ptr<lve::LveBuffer>> vertexBuffers; //one per frame in flight, kept mapped
		std::vector<Vertex> vertices; //scratch, reused every frame
	};
}
