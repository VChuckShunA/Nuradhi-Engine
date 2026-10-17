#pragma once

//std
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

//A small entity component system. Entities are ids, components are plain structs stored per type in
//packed arrays, and systems are ordinary functions that iterate the entities having a set of components:
//
//    registry.each<Transform, RigidBody>([](Entity entity, Transform& transform, RigidBody& body) { ... });
//
//Rule: don't add or remove components of the types being iterated (or destroy entities) inside each().
//Collect the entities and change them after the loop.
namespace nrd::ecs {

	//The low 20 bits index into the registry's tables; the high 12 bits count how often that slot has been reused,
	//so a handle to a destroyed entity can be told apart from the entity that later takes its slot.
	using Entity = std::uint32_t;
	inline constexpr Entity NullEntity = 0xFFFFFFFFu;

	namespace detail {
		inline constexpr std::uint32_t INDEX_BITS = 20;
		inline constexpr std::uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1u;
		inline constexpr std::uint32_t GENERATION_MASK = 0xFFFu;

		inline std::uint32_t indexOf(Entity entity) { return entity & INDEX_MASK; }
		inline std::uint32_t generationOf(Entity entity) { return entity >> INDEX_BITS; }
		inline Entity makeEntity(std::uint32_t index, std::uint32_t generation) { return (generation << INDEX_BITS) | index; }

		inline std::size_t nextComponentId() {
			static std::size_t counter = 0;
			return counter++;
		}
		//one id per component type, handed out the first time the type is used
		template <typename T>
		std::size_t componentId() {
			static const std::size_t id = nextComponentId();
			return id;
		}
	}

	class PoolBase {
	public:
		virtual ~PoolBase() = default;
		virtual void remove(Entity entity) = 0;
		virtual bool contains(Entity entity) const = 0;
		virtual std::size_t size() const = 0;
		virtual const std::vector<Entity>& entities() const = 0;
	};

	//Sparse set: `sparse` maps an entity's index to its slot in the packed `dense` and `components` arrays,
	//so lookups are O(1) and iterating walks contiguous memory. Removal swaps the last component into the hole.
	//Adding a component can reallocate the array, so don't keep component references across add() calls.
	template <typename T>
	class Pool final : public PoolBase {
	public:
		template <typename... Args>
		T& add(Entity entity, Args&&... args) {
			assert(!contains(entity) && "Entity already has this component");
			const std::uint32_t index = detail::indexOf(entity);
			if (index >= sparse.size()) sparse.resize(index + 1, NO_SLOT);
			sparse[index] = static_cast<std::uint32_t>(dense.size());
			dense.push_back(entity);
			if constexpr (std::is_aggregate_v<T>) {
				components.push_back(T{ std::forward<Args>(args)... });
			}
			else {
				components.emplace_back(std::forward<Args>(args)...);
			}
			return components.back();
		}

		void remove(Entity entity) override {
			if (!contains(entity)) return;
			const std::uint32_t slot = sparse[detail::indexOf(entity)];
			const std::uint32_t last = static_cast<std::uint32_t>(dense.size() - 1);
			if (slot != last) {
				dense[slot] = dense[last];
				components[slot] = std::move(components[last]);
				sparse[detail::indexOf(dense[slot])] = slot;
			}
			dense.pop_back();
			components.pop_back();
			sparse[detail::indexOf(entity)] = NO_SLOT;
		}

		bool contains(Entity entity) const override {
			const std::uint32_t index = detail::indexOf(entity);
			return index < sparse.size() && sparse[index] != NO_SLOT && dense[sparse[index]] == entity;
		}

		T* tryGet(Entity entity) { return contains(entity) ? &components[sparse[detail::indexOf(entity)]] : nullptr; }
		const T* tryGet(Entity entity) const { return contains(entity) ? &components[sparse[detail::indexOf(entity)]] : nullptr; }
		T& get(Entity entity) {
			assert(contains(entity) && "Entity doesn't have this component");
			return components[sparse[detail::indexOf(entity)]];
		}

		std::size_t size() const override { return dense.size(); }
		const std::vector<Entity>& entities() const override { return dense; }

	private:
		static constexpr std::uint32_t NO_SLOT = 0xFFFFFFFFu;

		std::vector<std::uint32_t> sparse;
		std::vector<Entity> dense;
		std::vector<T> components;
	};

	class Registry {
	public:
		Entity create() {
			if (!freeIndices.empty()) {
				const std::uint32_t index = freeIndices.back();
				freeIndices.pop_back();
				return detail::makeEntity(index, generations[index]);
			}
			const std::uint32_t index = static_cast<std::uint32_t>(generations.size());
			assert(index < detail::INDEX_MASK && "Out of entity ids");
			generations.push_back(0);
			return detail::makeEntity(index, 0);
		}

		//removes every component and frees the id; old handles to it become invalid
		void destroy(Entity entity) {
			if (!valid(entity)) return;
			for (auto& pool : pools) {
				if (pool) pool->remove(entity);
			}
			const std::uint32_t index = detail::indexOf(entity);
			generations[index] = (generations[index] + 1) & detail::GENERATION_MASK;
			freeIndices.push_back(index);
		}

		bool valid(Entity entity) const {
			const std::uint32_t index = detail::indexOf(entity);
			return entity != NullEntity && index < generations.size() && generations[index] == detail::generationOf(entity);
		}

		//aggregates are brace-initialised from args (registry.add<RigidBody>(e) or add<Transform>(e, Transform{...}));
		//other types are constructed with them
		template <typename T, typename... Args>
		T& add(Entity entity, Args&&... args) {
			assert(valid(entity) && "Adding a component to an invalid entity");
			return pool<T>().add(entity, std::forward<Args>(args)...);
		}

		template <typename T>
		void remove(Entity entity) {
			if (Pool<T>* found = findPool<T>()) found->remove(entity);
		}

		template <typename T>
		bool has(Entity entity) const {
			const Pool<T>* found = findPool<T>();
			return found && found->contains(entity);
		}

		template <typename T>
		T* tryGet(Entity entity) {
			Pool<T>* found = findPool<T>();
			return found ? found->tryGet(entity) : nullptr;
		}

		template <typename T>
		T& get(Entity entity) {
			T* component = tryGet<T>(entity);
			assert(component && "Entity doesn't have this component");
			return *component;
		}

		//Calls fn(entity, components...) for every entity that has all of Ts, walking the smallest pool
		template <typename... Ts, typename Fn>
		void each(Fn&& fn) {
			static_assert(sizeof...(Ts) > 0, "each() needs at least one component type");
			static_assert((!std::is_const_v<Ts> && ...), "Ask for components without const");

			const std::tuple<Pool<Ts>*...> found{ findPool<Ts>()... };
			if (((std::get<Pool<Ts>*>(found) == nullptr) || ...)) return;

			const PoolBase* smallest = nullptr;
			((smallest = (!smallest || std::get<Pool<Ts>*>(found)->size() < smallest->size()) ? std::get<Pool<Ts>*>(found) : smallest), ...);

			const std::vector<Entity>& candidates = smallest->entities();
			for (std::size_t i = 0; i < candidates.size(); i++) {
				const Entity entity = candidates[i];
				if ((std::get<Pool<Ts>*>(found)->contains(entity) && ...)) {
					fn(entity, std::get<Pool<Ts>*>(found)->get(entity)...);
				}
			}
		}

		//first entity that has all of Ts, or NullEntity; handy for singletons like the active camera
		template <typename... Ts>
		Entity findFirst() {
			Entity result = NullEntity;
			each<Ts...>([&](Entity entity, Ts&...) {
				if (result == NullEntity) result = entity;
			});
			return result;
		}

		template <typename T>
		std::size_t count() const {
			const Pool<T>* found = findPool<T>();
			return found ? found->size() : 0;
		}

	private:
		template <typename T>
		Pool<T>& pool() {
			const std::size_t id = detail::componentId<T>();
			if (id >= pools.size()) pools.resize(id + 1);
			if (!pools[id]) pools[id] = std::make_unique<Pool<T>>();
			return static_cast<Pool<T>&>(*pools[id]);
		}

		template <typename T>
		Pool<T>* findPool() const {
			const std::size_t id = detail::componentId<T>();
			return id < pools.size() && pools[id] ? static_cast<Pool<T>*>(pools[id].get()) : nullptr;
		}

		std::vector<std::uint32_t> generations; //current generation of each entity slot
		std::vector<std::uint32_t> freeIndices; //slots of destroyed entities, reused first
		std::vector<std::unique_ptr<PoolBase>> pools; //indexed by detail::componentId<T>()
	};
}
