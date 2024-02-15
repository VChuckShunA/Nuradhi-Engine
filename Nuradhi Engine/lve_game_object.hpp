#pragma once

#include "lve_model.hpp"

//libs
#include <glm/gtc/matrix_transform.hpp>
//std
#include <memory>
#include<unordered_map>

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
		glm::mat4 mat4();

		glm::mat3 normalMatrix();

	};

	struct PointLightComponent {
		float lightIntensity = 1.0f;
	};
	class LveGameObject {
	public:
		using id_t = unsigned int;
		using Map = std::unordered_map<id_t,LveGameObject>;//used to look up game objects using their ID

		static LveGameObject createGameObject() {
			static id_t currentId = 0;
			return LveGameObject{currentId++};
		}

		static LveGameObject makePointLight(
			float intensity = 10.f, float radius = 0.1f, glm::vec3 colour = glm::vec3(1.f));

		//delete copy constructor and assignment operator to avoid having duplicate game objects
		LveGameObject(const LveGameObject&) = delete;
		LveGameObject& operator=(const LveGameObject&) = delete;
		//enable move constructor and assignment operator to use the default
		LveGameObject(LveGameObject&&) = default;
		LveGameObject& operator=(LveGameObject&&) = default;

		const id_t getId(){return id;}

		glm::vec3 colour{};
		TransformComponent transform{};

		//Optional pointer components
		std::shared_ptr<LveModel> model{};
		std::unique_ptr<PointLightComponent> pointLight = nullptr;

	private:
		LveGameObject(id_t objId) : id{objId} {}

		id_t id;
	};
}