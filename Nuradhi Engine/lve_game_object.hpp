#pragma once

#include "lve_model.hpp"

//libs
#include <glm/gtc/matrix_transform.hpp>
//std
#include <memory>

namespace lve {

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
		glm::mat4 mat4() {
			const float c3 = glm::cos(rotation.z);
			const float s3 = glm::sin(rotation.z);
			const float c2 = glm::cos(rotation.x);
			const float s2 = glm::sin(rotation.x);
			const float c1 = glm::cos(rotation.y);
			const float s1 = glm::sin(rotation.y);
			return glm::mat4{
				{
					scale.x * (c1 * c3 + s1 * s2 * s3),
					scale.x * (c2 * s3),
					scale.x * (c1 * s2 * s3 - c3 * s1),
					0.0f,
				},
				{
					scale.y * (c3 * s1 * s2 - c1 * s3),
					scale.y * (c2 * c3),
					scale.y * (c1 * c3 * s2 + s1 * s3),
					0.0f,
				},
				{
					scale.z * (c2 * s1),
					scale.z * (-s2),
					scale.z * (c1 * c2),
					0.0f,
				},
				{translation.x, translation.y, translation.z, 1.0f} };
		}
	};

	class LveGameObject {
	public:
		using id_it = unsigned int;

		static LveGameObject createGameObject() {
			static id_it currentId = 0;
			return LveGameObject{currentId++};
		}

		//delete copy constructor and assignment operator to avoid having duplicate game objects
		LveGameObject(const LveGameObject&) = delete;
		LveGameObject& operator=(const LveGameObject&) = delete;
		//enable move constructor and assignment operator to use the default
		LveGameObject(LveGameObject&&) = default;
		LveGameObject& operator=(LveGameObject&&) = default;

		const id_it getId(){return id;}

		std::shared_ptr<LveModel> model{};
		glm::vec3 colour{};
		TransformComponent transform{};

	private:
		LveGameObject(id_it objId) : id{objId} {}

		id_it id;
	};
}