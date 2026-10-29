#pragma once

#include "engine/ecs/registry.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nrd {

	struct FogVolume;

	//A coarse fluid simulation of a FogVolume, so characters can push it around. The grid covers the volume and
	//is anchored in the world: when the volume follows the camera, the grid scrolls a whole cell at a time instead
	//of dragging the fog along with it.
	//
	//Each step:
	//  1. every Sprite stamps the opaque pixels of its current frame into the grid as a solid obstacle. Blank pixels
	//     stay open, so fog flows between the legs and under an outstretched arm. Obstacles move with the
	//     character, and cells a pose change newly fills (a kick, a punch) shove outwards from the body;
	//  2. the flow is carried along by itself (semi-Lagrangian advection), stirred to keep eddies alive (vorticity
	//     confinement), damped, and made incompressible (a Jacobi pressure solve), so pushed fog has to go around;
	//  3. fog density and a displacement field ride on the flow. Fog inside an obstacle is cleared and drifts back
	//     in over refillTime; the displacement drags the fog's noise pattern, so the billows visibly move, then
	//     relaxes back.
	//
	//Grid axes are x, height above the ground, and z. Velocities inside the grid use "up" as positive height.
	class FogSimulation {
	public:
		static constexpr int NX = 72; //across
		static constexpr int NY = 12; //up
		static constexpr int NZ = 24; //into the screen
		static constexpr int CELLS = NX * NY * NZ;

		FogSimulation();

		void step(ecs::Registry& registry, const glm::vec3& ground, const FogVolume& fog, float dt);

		//the state for the fog shader, NX x NY x NZ texels of 4 half floats, x fastest: density (1 is undisturbed),
		//then the displacement of the fog's pattern in world units (x, y, z; +y down)
		const std::vector<std::uint16_t>& texels() const { return packed; }
		//world position of the grid's corner (lowest x and z, at the ground) and its size (x, height, z)
		glm::vec3 origin() const { return gridOrigin; }
		glm::vec3 size() const { return gridSize; }

	private:
		struct Track {
			glm::vec3 position;
			glm::vec3 velocity{ 0.f }; //grid axes: x, up, z
			bool seen = false;
		};

		static int index(int i, int j, int k) { return i + NX * (j + NY * k); }

		void resize(const glm::vec3& ground, const FogVolume& fog);
		void scrollTo(float originX);
		void stampObstacles(ecs::Registry& registry, const FogVolume& fog, float dt);
		void addVorticity(float strength, float dt);
		void project();
		void enforceObstacles();
		void pack();

		glm::vec3 cell{ 1.f }; //cell size along x, up, z
		glm::vec3 gridOrigin{ 0.f };
		glm::vec3 gridSize{ 0.f };
		glm::vec3 volumeShape{ 0.f }; //the volume's half width, height and half depth the grid was built for

		//velocity (x, up, z) and scratch copies for advection
		std::vector<float> u, v, w, u0, v0, w0;
		std::vector<float> density, displacementX, displacementY, displacementZ, scratch;
		std::vector<float> occupancy, previousOccupancy, obstacleU, obstacleV, obstacleW;
		std::vector<float> pressure, pressureNext, divergence, curlX, curlY, curlZ, curlLength;
		std::vector<std::uint16_t> packed;
		std::unordered_map<ecs::Entity, Track> tracks;
	};
}
