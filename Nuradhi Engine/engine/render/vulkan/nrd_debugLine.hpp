#pragma once
#include "lve_device.hpp"
#include "lve_buffer.hpp"
//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

//std 
#include <memory>
#include <vector>
namespace nrd {
	class NrdDebugLine
	{
	public:

		struct Vertex {
			glm::vec3 position{};
			glm::vec3 colour{};
			glm::vec3 normal{};
			glm::vec2 uv{}; //2d texture  cordinates

			static std::vector<VkVertexInputBindingDescription> getBindingDescription();
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

			bool operator==(const Vertex& other) const {
				return position == other.position && colour == other.colour && normal == other.normal && uv == other.uv;
			}

		};

		struct Builder {
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};

			//void loadModel(const std::string& filepath);
		};

		NrdDebugLine(lve::LveDevice& device, const NrdDebugLine::Builder& builder);
		~NrdDebugLine();

		//Delete these cuz the model class manages the buffer and memory objects
		NrdDebugLine(const NrdDebugLine&) = delete;
		NrdDebugLine& operator=(const NrdDebugLine&) = delete;


		void bind(VkCommandBuffer commandBuffer);
		void draw(VkCommandBuffer commandBuffer);

	private:
		void createVertexBuffers(const std::vector<Vertex>& vertices);
		void createIndexBuffers(const std::vector<uint32_t>& indices);
		lve::LveDevice& lveDevice;

		std::unique_ptr<lve::LveBuffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false; //so we can use bothe vertex and index buffers
		std::unique_ptr<lve::LveBuffer> indexBuffer;
		uint32_t indexCount;

	};


}