#pragma once

#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/nrd_debugLine.hpp"

//std
#include <memory>

namespace nrd {

	//Draws every Collider as a wire box at its true world bounds, so what you see is what collision tests.
	//Render it with the wireframe pipeline (polygon mode LINE): each edge is a degenerate triangle.
	class DebugRenderSystem {
	public:
		explicit DebugRenderSystem(lve::LveDevice& device);

		DebugRenderSystem(const DebugRenderSystem&) = delete;
		DebugRenderSystem& operator=(const DebugRenderSystem&) = delete;

		void renderColliders(lve::FrameInfo& frameInfo, VkPipelineLayout pipelineLayout);

	private:
		std::unique_ptr<NrdDebugLine> unitCube; //the 12 edges of the [-1, 1] cube, shared by every collider
	};
}
