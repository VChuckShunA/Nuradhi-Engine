#pragma once

#include "lve_model.hpp"

//std
#include <memory>

namespace lve {

	struct Transform2dComponent
	{
		glm::vec2 translation{}; //(position offset)
		glm::vec2 scale{ 1.f,1.f };
		float rotation;

		glm::mat2 mat2() { 
			const float s = glm::sin(rotation);
			const float c = glm::cos(rotation);
			glm::mat2 rotMatrix{ {c,s},{-s,c} };

			glm::mat2 scaleMat{ {scale.x,.0f},{.0f,scale.y} };
			return rotMatrix* scaleMat;
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
		Transform2dComponent transform2d{};

	private:
		LveGameObject(id_it objId) : id{objId} {}

		id_it id;
	};
}