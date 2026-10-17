#pragma once

#include "lve_device.hpp"
#include "lve_frame_info.hpp"
#include "nrd_collision.hpp"
#include "nrd_debugLine.hpp"

//std
#include <memory>

namespace nrd {
	//Draws every collider in a CollisionWorld as a wire box at its true world bounds, so what you see is what
	//collision tests. Render it with the debug line pipeline (polygon mode LINE): each edge is a degenerate triangle.
	class ColliderDebugRenderer {
	public:
		explicit ColliderDebugRenderer(lve::LveDevice& device);

		ColliderDebugRenderer(const ColliderDebugRenderer&) = delete;
		ColliderDebugRenderer& operator=(const ColliderDebugRenderer&) = delete;

		void render(lve::FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const CollisionWorld& world);

	private:
		std::unique_ptr<NrdDebugLine> unitCube; //the 12 edges of the [-1, 1] cube, shared by every collider
	};
}
