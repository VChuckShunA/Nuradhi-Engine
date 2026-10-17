#include "engine/render/debug_render_system.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/collision.hpp"

//libs
#include <glm/gtc/matrix_transform.hpp>

//std
#include <array>

namespace nrd {

	DebugRenderSystem::DebugRenderSystem(lve::LveDevice& device)
	{
		const std::array<glm::vec3, 8> corners{ {
			{-1.f, -1.f, -1.f}, {1.f, -1.f, -1.f}, {1.f, 1.f, -1.f}, {-1.f, 1.f, -1.f},
			{-1.f, -1.f,  1.f}, {1.f, -1.f,  1.f}, {1.f, 1.f,  1.f}, {-1.f, 1.f,  1.f},
		} };
		const std::array<std::array<int, 2>, 12> edges{ {
			{0, 1}, {1, 2}, {2, 3}, {3, 0}, //front face
			{4, 5}, {5, 6}, {6, 7}, {7, 4}, //back face
			{0, 4}, {1, 5}, {2, 6}, {3, 7}, //connecting edges
		} };

		NrdDebugLine::Builder builder{};
		const glm::vec3 colour{ 1.f, 0.f, 0.f };
		for (const auto& edge : edges) {
			//a degenerate triangle a-b-a renders as the single line a-b in polygon mode LINE, and as nothing when filled
			builder.vertices.push_back({ corners[edge[0]], colour, {}, {} });
			builder.vertices.push_back({ corners[edge[1]], colour, {}, {} });
			builder.vertices.push_back({ corners[edge[0]], colour, {}, {} });
		}
		unitCube = std::make_unique<NrdDebugLine>(device, builder);
	}

	void DebugRenderSystem::renderColliders(lve::FrameInfo& frameInfo, VkPipelineLayout pipelineLayout)
	{
		unitCube->bind(frameInfo.commandBuffer);
		frameInfo.registry.each<Transform, Collider>([&](ecs::Entity, Transform& transform, Collider& collider) {
			const Aabb bounds = collider.worldBounds(transform.position);

			lve::SimplePushConstantData push{};
			push.modelMatrix = glm::scale(glm::translate(glm::mat4{ 1.f }, bounds.center()), bounds.halfExtents());
			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(lve::SimplePushConstantData),
				&push);
			unitCube->draw(frameInfo.commandBuffer);
		});
	}
}
