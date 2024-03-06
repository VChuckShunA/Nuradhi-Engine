#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>

#include "nrd_debugLine.hpp"
#include "lve_frame_info.hpp"


namespace nrd {
	class NrdDebugLine;
	class NrdSweptAABB {
	public:
		float x1, x2, x3, x4;
		float y1, y2, y3, y4;
		float zf1, zf2, zf3, zf4, zb1, zb2, zb3, zb4; 
		nrd::NrdDebugLine::Builder debugLineBuilder{};
		glm::vec3 pointTBL,pointTBR, pointTFL, pointTFR, pointBBL, pointBBR, pointBFL, pointBFR; 
		NrdSweptAABB(lve::LveDevice& lveDevice);
		NrdSweptAABB(lve::LveDevice& lveDevice,
					float x1, float x2, float x3, float x4, 
					float y1, float y2, float y3, float y4, 
					float zf1, float zf2, float zf3, float zf4,
					float zb1, float zb2, float zb3, float zb4);

		~NrdSweptAABB();
		void createPoint(float x,float y, float z, glm::vec3& point);

		void createBox(
						glm::vec3& point1,
						glm::vec3& point2, 
						glm::vec3& point3, 
						glm::vec3& point4, 
						glm::vec3& point5, 
						glm::vec3& point6, 
						glm::vec3& point7, 
						glm::vec3& point8);

		void createLine(glm::vec3& point1,glm::vec3& point2);
		std::unique_ptr<nrd::NrdDebugLine> colliderBox = nullptr;
		nrd::NrdDebugLine::Builder getCollider() { return debugLineBuilder; }
		void destroyDebugLine() { colliderBox = nullptr; }
	private:
	};
}