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

	//Draws BloodEffect droplets (camera-facing quads stretched along their motion) and splats (quads lying flat on
	//the floor), alpha blended without writing depth. The quads are built on the CPU each frame into a per-frame
	//vertex buffer.
	class BloodRenderSystem {
	public:
		static constexpr std::uint32_t MAX_QUADS = 3000; //droplets and splats per frame

		BloodRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~BloodRenderSystem();

		BloodRenderSystem(const BloodRenderSystem&) = delete;
		BloodRenderSystem& operator=(const BloodRenderSystem&) = delete;

		void render(lve::FrameInfo& frameInfo);

		struct Vertex {
			glm::vec3 position;
			glm::vec2 uv;
			glm::vec4 data; //x 0 for a droplet, 1 for a splat; y opacity; z a random seed for the splat's shape
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
