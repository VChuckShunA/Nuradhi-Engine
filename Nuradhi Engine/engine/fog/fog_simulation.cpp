#include "engine/fog/fog_simulation.hpp"
#include "engine/core/components.hpp"
#include "engine/fog/fog_volume.hpp"
#include "engine/physics/collision.hpp"
#include "engine/physics/physics.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <glm/gtc/packing.hpp>

//std
#include <algorithm>
#include <cmath>
#include <optional>

//The solver works in grid units (cells and cells per second) with every cell treated as a cube. The real cells
//are flat (the fog is far wider than it is tall), and solving with their true proportions would make pressure
//spread almost only vertically, so pushed fog would pile up instead of flowing around a leg. World velocities are
//converted to cells per second on the way in and displacements back to world units on the way out.
//Hot loops index raw pointers: checked std::vector access makes Debug builds several times slower.

namespace nrd {

	namespace {
		constexpr int PRESSURE_ITERATIONS = 24;
		constexpr float DAMPING = 1.2f; //per second: stirred fog comes to rest in a second or two
		constexpr float MAX_BODY_SPEED = 30.f; //cells per second; faster than this is a teleport, not movement
		constexpr float MAX_DISPLACEMENT = 1.2f; //world units the fog's pattern can be dragged

		using Grid = FogSimulation;

		//trilinear sample at a point in cell coordinates (cell centres are whole numbers), clamped to the grid
		float sample(const float* field, float x, float y, float z)
		{
			x = std::clamp(x, 0.f, Grid::NX - 1.001f);
			y = std::clamp(y, 0.f, Grid::NY - 1.001f);
			z = std::clamp(z, 0.f, Grid::NZ - 1.001f);
			const int i = static_cast<int>(x), j = static_cast<int>(y), k = static_cast<int>(z);
			const float fx = x - i, fy = y - j, fz = z - k;
			const auto at = [field](int a, int b, int c) { return field[a + Grid::NX * (b + Grid::NY * c)]; };
			const float c00 = at(i, j, k) + (at(i + 1, j, k) - at(i, j, k)) * fx;
			const float c10 = at(i, j + 1, k) + (at(i + 1, j + 1, k) - at(i, j + 1, k)) * fx;
			const float c01 = at(i, j, k + 1) + (at(i + 1, j, k + 1) - at(i, j, k + 1)) * fx;
			const float c11 = at(i, j + 1, k + 1) + (at(i + 1, j + 1, k + 1) - at(i, j + 1, k + 1)) * fx;
			const float c0 = c00 + (c10 - c00) * fy;
			const float c1 = c01 + (c11 - c01) * fy;
			return c0 + (c1 - c0) * fz;
		}

		//semi-Lagrangian: each cell takes the value found where the flow carried it from
		void advectField(const float* source, float* destination, const float* u, const float* v, const float* w, float dt)
		{
			for (int k = 0; k < Grid::NZ; k++) {
				for (int j = 0; j < Grid::NY; j++) {
					for (int i = 0; i < Grid::NX; i++) {
						const int c = i + Grid::NX * (j + Grid::NY * k);
						destination[c] = sample(source, i - u[c] * dt, j - v[c] * dt, k - w[c] * dt);
					}
				}
			}
		}

		//moves a field `shift` cells along x (positive: the grid moved right), filling the new cells with `rest`
		void shiftAlongX(std::vector<float>& field, int shift, float rest)
		{
			float* f = field.data();
			for (int k = 0; k < Grid::NZ; k++) {
				for (int j = 0; j < Grid::NY; j++) {
					float* row = f + Grid::NX * (j + Grid::NY * k);
					if (shift > 0) {
						for (int i = 0; i < Grid::NX; i++) row[i] = i + shift < Grid::NX ? row[i + shift] : rest;
					}
					else {
						for (int i = Grid::NX - 1; i >= 0; i--) row[i] = i + shift >= 0 ? row[i + shift] : rest;
					}
				}
			}
		}
	}

	FogSimulation::FogSimulation()
	{
		for (std::vector<float>* field : { &u, &v, &w, &u0, &v0, &w0, &displacementX, &displacementY, &displacementZ, &scratch,
			&occupancy, &previousOccupancy, &obstacleU, &obstacleV, &obstacleW, &pressure, &pressureNext, &divergence,
			&curlX, &curlY, &curlZ, &curlLength }) {
			field->assign(CELLS, 0.f);
		}
		density.assign(CELLS, 1.f);
		packed.assign(static_cast<size_t>(CELLS) * 4, 0);
	}

	void FogSimulation::resize(const glm::vec3& ground, const FogVolume& fog)
	{
		volumeShape = { fog.halfExtents.x, fog.height, fog.halfExtents.y };
		cell = { 2.f * fog.halfExtents.x / NX, fog.height / NY, 2.f * fog.halfExtents.y / NZ };
		gridSize = { 2.f * fog.halfExtents.x, fog.height, 2.f * fog.halfExtents.y };
		gridOrigin = { std::floor((ground.x - fog.halfExtents.x) / cell.x) * cell.x, ground.y, ground.z - fog.halfExtents.y };
		for (std::vector<float>* field : { &u, &v, &w, &displacementX, &displacementY, &displacementZ, &occupancy, &previousOccupancy, &pressure }) {
			std::fill(field->begin(), field->end(), 0.f);
		}
		std::fill(density.begin(), density.end(), 1.f);
		tracks.clear();
	}

	void FogSimulation::scrollTo(float originX)
	{
		const int shift = static_cast<int>(std::lround((originX - gridOrigin.x) / cell.x));
		if (shift == 0) return;
		gridOrigin.x += shift * cell.x;
		for (std::vector<float>* field : { &u, &v, &w, &displacementX, &displacementY, &displacementZ, &occupancy, &previousOccupancy, &pressure }) {
			if (std::abs(shift) >= NX) std::fill(field->begin(), field->end(), 0.f);
			else shiftAlongX(*field, shift, 0.f);
		}
		if (std::abs(shift) >= NX) std::fill(density.begin(), density.end(), 1.f);
		else shiftAlongX(density, shift, 1.f);
	}

	void FogSimulation::step(ecs::Registry& registry, const glm::vec3& ground, const FogVolume& fog, float dt)
	{
		if (dt <= 0.f) return;
		dt = std::min(dt, 1.f / 30.f); //a long frame (a breakpoint, a window drag) shouldn't blow the fog away
		if (glm::vec3{ fog.halfExtents.x, fog.height, fog.halfExtents.y } != volumeShape) resize(ground, fog);
		gridOrigin.y = ground.y;
		gridOrigin.z = ground.z - fog.halfExtents.y;
		scrollTo(std::floor((ground.x - fog.halfExtents.x) / cell.x) * cell.x);

		//1. characters
		stampObstacles(registry, fog, dt);
		enforceObstacles();

		//2. flow: carried by itself, stirred, damped, then made incompressible around the obstacles
		std::copy(u.begin(), u.end(), u0.begin());
		std::copy(v.begin(), v.end(), v0.begin());
		std::copy(w.begin(), w.end(), w0.begin());
		advectField(u0.data(), u.data(), u0.data(), v0.data(), w0.data(), dt);
		advectField(v0.data(), v.data(), u0.data(), v0.data(), w0.data(), dt);
		advectField(w0.data(), w.data(), u0.data(), v0.data(), w0.data(), dt);
		addVorticity(fog.swirl * 6.f, dt);
		const float damping = std::exp(-DAMPING * dt);
		for (int c = 0; c < CELLS; c++) {
			u[c] *= damping;
			v[c] *= damping;
			w[c] *= damping;
		}
		enforceObstacles();
		project();

		//3. what rides on the flow
		const float relax = 1.f - std::exp(-dt / std::max(fog.refillTime, 0.05f));
		advectField(density.data(), scratch.data(), u.data(), v.data(), w.data(), dt);
		std::swap(density, scratch);
		for (std::vector<float>* field : { &displacementX, &displacementY, &displacementZ }) {
			advectField(field->data(), scratch.data(), u.data(), v.data(), w.data(), dt);
			std::swap(*field, scratch);
		}
		float* d = density.data();
		const float* occupied = occupancy.data();
		float* dx = displacementX.data();
		float* dy = displacementY.data();
		float* dz = displacementZ.data();
		for (int c = 0; c < CELLS; c++) {
			d[c] += (1.f - d[c]) * relax; //drifts back in
			d[c] *= 1.f - occupied[c]; //and none inside a body
			dx[c] = (dx[c] + u[c] * dt) * (1.f - relax);
			dy[c] = (dy[c] + v[c] * dt) * (1.f - relax);
			dz[c] = (dz[c] + w[c] * dt) * (1.f - relax);
		}
		pack();
	}

	void FogSimulation::stampObstacles(ecs::Registry& registry, const FogVolume& fog, float dt)
	{
		std::swap(previousOccupancy, occupancy);
		std::fill(occupancy.begin(), occupancy.end(), 0.f);
		for (auto& [entity, track] : tracks) track.seen = false;

		const glm::vec3 toCells{ 1.f / cell.x, -1.f / cell.y, 1.f / cell.z }; //world velocity to grid velocity (up is +)
		float* occupied = occupancy.data();
		const float* previous = previousOccupancy.data();

		registry.each<Transform, Sprite>([&](ecs::Entity entity, Transform& transform, Sprite& sprite) {
			//how the character moved since the last step
			auto [found, isNew] = tracks.try_emplace(entity, Track{ transform.position });
			Track& track = found->second;
			glm::vec3 velocity{ 0.f };
			if (!isNew) {
				velocity = (transform.position - track.position) / dt * toCells;
				if (glm::length(velocity) > MAX_BODY_SPEED * 4.f) velocity = glm::vec3{ 0.f }; //teleported
				velocity = glm::clamp(velocity, glm::vec3{ -MAX_BODY_SPEED }, glm::vec3{ MAX_BODY_SPEED });
			}
			const glm::vec3 lastVelocity = track.velocity;
			track.position = transform.position;
			track.velocity = velocity;
			track.seen = true;
			velocity *= fog.bodyPush;

			//the quad the sprite is drawn on (bottom-centre origin, scaled to the frame)
			const Collider* collider = registry.tryGet<Collider>(entity);
			const std::optional<Aabb> body = collider ? std::optional<Aabb>{ collider->worldBounds(transform.position) } : std::nullopt;
			const Transform quad = sprite.frameTransform(transform.position, body ? &*body : nullptr);
			const SpriteFrame& frame = sprite.animator.currentFrame();
			const SpriteSheet& sheet = sprite.animator.sheet();
			const float left = quad.position.x - quad.scale.x * 0.5f;
			const float bottomHeight = gridOrigin.y - quad.position.y; //height of the frame's bottom edge above the ground
			const float topHeight = bottomHeight + quad.scale.y;

			const int i0 = std::max(0, static_cast<int>(std::floor((left - gridOrigin.x) / cell.x)));
			const int i1 = std::min(NX - 1, static_cast<int>(std::floor((left + quad.scale.x - gridOrigin.x) / cell.x)));
			const int j0 = std::max(0, static_cast<int>(std::floor(bottomHeight / cell.y)));
			const int j1 = std::min(NY - 1, static_cast<int>(std::floor(topHeight / cell.y)));
			const float halfThickness = fog.bodyThickness * 0.5f + cell.z * 0.5f;
			const int k0 = std::max(0, static_cast<int>(std::floor((transform.position.z - halfThickness - gridOrigin.z) / cell.z)));
			const int k1 = std::min(NZ - 1, static_cast<int>(std::floor((transform.position.z + halfThickness - gridOrigin.z) / cell.z)));
			if (i0 > i1 || j0 > j1 || k0 > k1) return; //not in the fog

			//body centre in cell coordinates, for pushing limbs outwards
			const float centreI = (transform.position.x - gridOrigin.x) / cell.x - 0.5f;
			const float centreK = (transform.position.z - gridOrigin.z) / cell.z - 0.5f;

			for (int j = j0; j <= j1; j++) {
				for (int i = i0; i <= i1; i++) {
					//how much of this cell the frame's opaque pixels fill, from a 3 x 3 grid of samples
					int filled = 0;
					for (int sy = 0; sy < 3; sy++) {
						for (int sx = 0; sx < 3; sx++) {
							const float x = gridOrigin.x + (i + (sx + 0.5f) / 3.f) * cell.x;
							const float height = (j + (sy + 0.5f) / 3.f) * cell.y;
							float across = (x - left) / quad.scale.x;
							const float down = (topHeight - height) / quad.scale.y; //0 at the frame's top row
							if (across < 0.f || across >= 1.f || down < 0.f || down >= 1.f) continue;
							if (sprite.flipX) across = 1.f - across;
							const int px = frame.position.x + static_cast<int>(across * frame.size.x);
							const int py = frame.position.y + static_cast<int>(down * frame.size.y);
							if (sheet.isOpaque(px, py)) filled++;
						}
					}
					if (filled == 0) continue;
					const float coverage = filled / 9.f;

					for (int k = k0; k <= k1; k++) {
						const int c = index(i, j, k);
						glm::vec3 push = velocity;
						//a pose change that fills a cell (a kick, a punch) shoves it away from the body
						const float grown = coverage - previous[c];
						if (grown > 0.05f) {
							glm::vec2 away{ i - centreI, k - centreK };
							const float length = glm::length(away);
							away = length > 0.01f ? away / length : glm::vec2{ sprite.flipX ? -1.f : 1.f, 0.f };
							push += glm::vec3{ away.x, 0.f, away.y } * (fog.limbPush * 40.f * grown);
						}
						if (coverage >= occupied[c]) {
							occupied[c] = coverage;
							obstacleU[c] = push.x;
							obstacleV[c] = push.y;
							obstacleW[c] = push.z;
						}
					}
				}
			}

			//landing from a jump blows the fog out from under the feet
			const RigidBody* rigidBody = registry.tryGet<RigidBody>(entity);
			const bool landed = rigidBody && rigidBody->grounded && lastVelocity.y < -6.f && velocity.y > -1.f;
			if (landed && fog.landingPuff > 0.f) {
				const float strength = std::min(-lastVelocity.y, MAX_BODY_SPEED) * fog.landingPuff * 0.6f;
				const float radius = 0.7f; //world units
				const int ri = static_cast<int>(radius / cell.x) + 1, rk = static_cast<int>(radius / cell.z) + 1;
				for (int k = std::max(0, static_cast<int>(centreK) - rk); k <= std::min(NZ - 1, static_cast<int>(centreK) + rk); k++) {
					for (int i = std::max(0, static_cast<int>(centreI) - ri); i <= std::min(NX - 1, static_cast<int>(centreI) + ri); i++) {
						const glm::vec2 offset{ (i - centreI) * cell.x, (k - centreK) * cell.z }; //world units
						const float distance = glm::length(offset);
						if (distance > radius || distance < 1e-3f) continue;
						const glm::vec2 out = offset / distance * (strength * (1.f - distance / radius));
						for (int j = 0; j < std::min(NY, 5); j++) {
							const int c = index(i, j, k);
							u[c] += out.x;
							w[c] += out.y;
							v[c] += glm::length(out) * 0.25f; //and lifts it a little
						}
					}
				}
			}
		});

		for (auto it = tracks.begin(); it != tracks.end();) {
			it = it->second.seen ? std::next(it) : tracks.erase(it);
		}
	}

	void FogSimulation::enforceObstacles()
	{
		const float* o = occupancy.data();
		for (int c = 0; c < CELLS; c++) {
			if (o[c] <= 0.f) continue;
			u[c] += (obstacleU[c] - u[c]) * o[c];
			v[c] += (obstacleV[c] - v[c]) * o[c];
			w[c] += (obstacleW[c] - w[c]) * o[c];
		}
	}

	void FogSimulation::addVorticity(float strength, float dt)
	{
		if (strength <= 0.f) return;
		const float* U = u.data();
		const float* V = v.data();
		const float* W = w.data();
		//neighbours clamp at the edges
		const auto at = [](const float* f, int i, int j, int k) {
			return f[std::clamp(i, 0, NX - 1) + NX * (std::clamp(j, 0, NY - 1) + NY * std::clamp(k, 0, NZ - 1))];
		};
		for (int k = 0; k < NZ; k++) {
			for (int j = 0; j < NY; j++) {
				for (int i = 0; i < NX; i++) {
					const int c = index(i, j, k);
					curlX[c] = 0.5f * ((at(W, i, j + 1, k) - at(W, i, j - 1, k)) - (at(V, i, j, k + 1) - at(V, i, j, k - 1)));
					curlY[c] = 0.5f * ((at(U, i, j, k + 1) - at(U, i, j, k - 1)) - (at(W, i + 1, j, k) - at(W, i - 1, j, k)));
					curlZ[c] = 0.5f * ((at(V, i + 1, j, k) - at(V, i - 1, j, k)) - (at(U, i, j + 1, k) - at(U, i, j - 1, k)));
					curlLength[c] = std::sqrt(curlX[c] * curlX[c] + curlY[c] * curlY[c] + curlZ[c] * curlZ[c]);
				}
			}
		}
		//push along N x curl, where N points towards stronger rotation: small eddies spin up instead of smearing out
		const float* L = curlLength.data();
		for (int k = 0; k < NZ; k++) {
			for (int j = 0; j < NY; j++) {
				for (int i = 0; i < NX; i++) {
					const int c = index(i, j, k);
					glm::vec3 n{
						0.5f * (at(L, i + 1, j, k) - at(L, i - 1, j, k)),
						0.5f * (at(L, i, j + 1, k) - at(L, i, j - 1, k)),
						0.5f * (at(L, i, j, k + 1) - at(L, i, j, k - 1)) };
					const float length = glm::length(n);
					if (length < 1e-5f) continue;
					n /= length;
					const glm::vec3 force = glm::cross(n, glm::vec3{ curlX[c], curlY[c], curlZ[c] }) * strength;
					u[c] += force.x * dt;
					v[c] += force.y * dt;
					w[c] += force.z * dt;
				}
			}
		}
	}

	void FogSimulation::project()
	{
		//boundaries: the ground is solid (nothing flows into it, no pressure difference across it); the top and
		//sides are open air (velocity carries on, pressure is zero)
		const float* U = u.data();
		const float* V = v.data();
		const float* W = w.data();
		float* div = divergence.data();
		for (int k = 0; k < NZ; k++) {
			for (int j = 0; j < NY; j++) {
				for (int i = 0; i < NX; i++) {
					const int c = index(i, j, k);
					const float left = i > 0 ? U[c - 1] : U[c];
					const float right = i < NX - 1 ? U[c + 1] : U[c];
					const float below = j > 0 ? V[c - NX] : -V[c];
					const float above = j < NY - 1 ? V[c + NX] : V[c];
					const float back = k > 0 ? W[c - NX * NY] : W[c];
					const float front = k < NZ - 1 ? W[c + NX * NY] : W[c];
					div[c] = 0.5f * ((right - left) + (above - below) + (front - back));
				}
			}
		}

		std::fill(pressure.begin(), pressure.end(), 0.f);
		for (int iteration = 0; iteration < PRESSURE_ITERATIONS; iteration++) {
			const float* p = pressure.data();
			float* next = pressureNext.data();
			for (int k = 0; k < NZ; k++) {
				for (int j = 0; j < NY; j++) {
					for (int i = 0; i < NX; i++) {
						const int c = index(i, j, k);
						const float sum =
							(i > 0 ? p[c - 1] : 0.f) + (i < NX - 1 ? p[c + 1] : 0.f) +
							(j > 0 ? p[c - NX] : p[c]) + (j < NY - 1 ? p[c + NX] : 0.f) +
							(k > 0 ? p[c - NX * NY] : 0.f) + (k < NZ - 1 ? p[c + NX * NY] : 0.f);
						next[c] = (sum - div[c]) / 6.f;
					}
				}
			}
			std::swap(pressure, pressureNext);
		}

		const float* p = pressure.data();
		for (int k = 0; k < NZ; k++) {
			for (int j = 0; j < NY; j++) {
				for (int i = 0; i < NX; i++) {
					const int c = index(i, j, k);
					const float left = i > 0 ? p[c - 1] : 0.f;
					const float right = i < NX - 1 ? p[c + 1] : 0.f;
					const float below = j > 0 ? p[c - NX] : p[c];
					const float above = j < NY - 1 ? p[c + NX] : 0.f;
					const float back = k > 0 ? p[c - NX * NY] : 0.f;
					const float front = k < NZ - 1 ? p[c + NX * NY] : 0.f;
					u[c] -= 0.5f * (right - left);
					v[c] -= 0.5f * (above - below);
					w[c] -= 0.5f * (front - back);
				}
			}
		}
	}

	void FogSimulation::pack()
	{
		//displacement goes out in world units, +y down like the rest of the engine
		std::uint16_t* out = packed.data();
		for (int c = 0; c < CELLS; c++) {
			glm::vec3 displacement{ displacementX[c] * cell.x, -displacementY[c] * cell.y, displacementZ[c] * cell.z };
			const float length = glm::length(displacement);
			if (length > MAX_DISPLACEMENT) displacement *= MAX_DISPLACEMENT / length;
			out[c * 4 + 0] = glm::packHalf1x16(density[c]);
			out[c * 4 + 1] = glm::packHalf1x16(displacement.x);
			out[c * 4 + 2] = glm::packHalf1x16(displacement.y);
			out[c * 4 + 3] = glm::packHalf1x16(displacement.z);
		}
	}
}
