#include "nrd_sweptAABB.hpp"
#include <glm/gtx/string_cast.hpp>
#include <iostream>
namespace nrd {
	NrdSweptAABB::NrdSweptAABB(lve::LveDevice& lveDevice)
	{
		glm::vec3 point1={ 8.5f, 2.5f, 0.0f };
		glm::vec3 point2 = { 1.0f, 0.0f, 0.0f };
		debugLineBuilder.vertices.push_back({ { point1 }, { point2 }, {}, {} });
		debugLineBuilder.vertices.push_back({ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, {}, {} });
		debugLineBuilder.vertices.push_back({ { point1 }, { point2 }, {}, {} });
		colliderBox = std::make_unique<NrdDebugLine>(lveDevice, debugLineBuilder);

		//draw debug lines here
		//debugline->bind(frameInfo.commandBuffer);
		//debugline->draw(frameInfo.commandBuffer);
	}
	NrdSweptAABB::NrdSweptAABB(lve::LveDevice& lveDevice,
		float x1, float x2, float x3, float x4, 
		float y1, float y2, float y3, float y4, 
		float zf1, float zf2, float zf3, float zf4, 
		float zb1, float zb2, float zb3, float zb4):
		x1(x1),x2(x2),x3(x3),x4(x4),
		y1(y1),y2(y2),y3(y3),y4(y4), 
		zf1(zf1), zf2(zf2), zf3(zf3),zf4(zf4), zb1(zb1), zb2(zb2), zb3(zb3), zb4(zb4)
	{
		createPoint(x1, y1, zf1, pointBFL);
		createPoint(x2, y2, zf2, pointTFL);
		createPoint(x3, y3, zf3, pointTFR);
		createPoint(x4, y4, zf4, pointBFR);
		createPoint(x1, y1, zb1, pointBBL);
		createPoint(x2, y2, zb2, pointTBL);
		createPoint(x3, y3, zb3, pointTBR);
		createPoint(x4, y4, zb4, pointBBR);
		
		createBox(pointBFL, pointTFL, pointTFR, pointBFR, pointBBL, pointTBL, pointTBR, pointBBR);
		colliderBox = std::make_unique<NrdDebugLine>(lveDevice, debugLineBuilder);
		// Set up vertices for a line, you may need to adjust these coordinates
	}
	NrdSweptAABB::~NrdSweptAABB()
	{
	}
	void NrdSweptAABB::createPoint(float x, float y, float z, glm::vec3& point)
	{
		point={x,z,y};
	}
	void NrdSweptAABB::createBox(glm::vec3& point1, glm::vec3& point2,glm::vec3& point3, glm::vec3& point4, glm::vec3& point5, glm::vec3& point6, glm::vec3& point7, glm::vec3& point8)
	{
		//bottom
		createLine(point1, point2);
		createLine(point2, point3);
		createLine(point3, point4);
		createLine(point4, point1);
		//top

		createLine(point5, point6);
		createLine(point6, point7);
		createLine(point7, point8);
		createLine(point8, point5);
		//connect

		createLine(point1, point5);
		createLine(point2, point6);
		createLine(point3, point7);
		createLine(point4, point8);

	}
	void NrdSweptAABB::createLine(glm::vec3& point1, glm::vec3& point2)
	{
		std::cout << glm::to_string(point1) << " " << glm::to_string(point2)<< "\n";
		debugLineBuilder.vertices.push_back({ point1, { 1.0f, 0.0f, 0.0f }, {}, {} });
		debugLineBuilder.vertices.push_back({ point2, { 1.0f, 0.0f, 0.0f }, {}, {} });
		debugLineBuilder.vertices.push_back({ point1, { 1.0f, 0.0f, 0.0f }, {}, {} });
	}
}
