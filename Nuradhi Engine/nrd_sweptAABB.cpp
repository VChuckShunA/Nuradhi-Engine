#include "nrd_sweptAABB.hpp"
#include <glm/gtx/string_cast.hpp>
#include <iostream>
namespace nrd {
	NrdSweptAABB::NrdSweptAABB()
	{
	}
	NrdSweptAABB::NrdSweptAABB(lve::LveDevice& lveDevice)
	{
		colliderBox = std::make_unique<NrdDebugLine>(lveDevice, debugLineBuilder);

		//draw debug lines here
		//debugline->bind(frameInfo.commandBuffer);
		//debugline->draw(frameInfo.commandBuffer);
	}
	std::list<NrdSweptAABB*> NrdSweptAABB::collisionBoxes;
	NrdSweptAABB::NrdSweptAABB(lve::LveDevice& lveDevice,
		float xMin, float xMax, float yMin, float yMax, float zMin, float zMax):
		xMin(xMin),xMax(xMax),yMin(yMin),yMax(yMax),zMin(zMin),zMax(zMax)
	{
		createPoint(xMin, yMin, zMin, pointBFL);
		createPoint(xMin, yMax, zMin, pointTFL);
		createPoint(xMax, yMax, zMin, pointTFR);
		createPoint(xMax, yMin, zMin, pointBFR);
		createPoint(xMin, yMin, zMax, pointBBL);
		createPoint(xMin, yMax, zMax, pointTBL);
		createPoint(xMax, yMax, zMax, pointTBR);
		createPoint(xMax, yMin, zMax, pointBBR);
		
		createBox(pointBFL, pointTFL, pointTFR, pointBFR, pointBBL, pointTBL, pointTBR, pointBBR);
		colliderBox = std::make_unique<NrdDebugLine>(lveDevice, debugLineBuilder);

		NrdSweptAABB::collisionBoxes.push_back(this);
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
		debugLineBuilder.vertices.push_back({ point1, { 1.0f, 0.0f, 0.0f }, {}, {} });
		debugLineBuilder.vertices.push_back({ point2, { 1.0f, 0.0f, 0.0f }, {}, {} });
		debugLineBuilder.vertices.push_back({ point1, { 1.0f, 0.0f, 0.0f }, {}, {} });
	}
}
