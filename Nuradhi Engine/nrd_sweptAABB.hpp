#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>

//libs
#include <glm/gtc/matrix_transform.hpp>
#include "nrd_debugLine.hpp"
#include "lve_frame_info.hpp"


namespace nrd {
	class NrdDebugLine;
	struct TransformComponent
	{
		glm::vec3 translation{}; //(position offset)
		glm::vec3 scale{ 1.f,1.f,1.f };
		glm::vec3 rotation{};

		//Matrix corresponds to translate * Ry * Rx * Rz * scale transformation
		//Rotation convention uses tait-bryan angles with axis order Y(1),X(2),Z(3) 
		//WARNING! Order(y,x,z) Matters! I think...
		// Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
  // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
  // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
		glm::mat4 mat4();

		glm::mat3 normalMatrix();

	};
	class NrdSweptAABB {
	public:
		TransformComponent transform{};
		float xMin, xMax, yMin, yMax, zMin, zMax;
		nrd::NrdDebugLine::Builder debugLineBuilder{};
		glm::vec3 pointTBL,pointTBR, pointTFL, pointTFR, pointBBL, pointBBR, pointBFL, pointBFR;
		NrdSweptAABB();
		NrdSweptAABB(lve::LveDevice& lveDevice);
		NrdSweptAABB(lve::LveDevice& lveDevice,float xMin, float xMax, float yMin, float yMax, float zMin, float zMax);

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
		static std::list<NrdSweptAABB*> collisionBoxes;

	private:
	};
}