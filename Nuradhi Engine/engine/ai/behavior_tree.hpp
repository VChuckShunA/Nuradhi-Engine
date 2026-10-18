#pragma once

#include "engine/ecs/registry.hpp"

//std
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

//Behaviour trees, after the NashCoreEngine design: every node is ticked by its parent and answers Success,
//Failure or Running. Composites decide which children to tick; leaves (conditions and actions) do the work.
//A tree belongs to one entity (nodes keep per-run state such as timers), lives in a BehaviorTree component, and
//the engine ticks it once per frame, after the game's Update systems and before physics.
namespace nrd::ai {

	enum class Status { Invalid, Success, Failure, Running, Aborted };

	//Per-entity memory the tree's nodes share, e.g. the current target or a cooldown
	class Blackboard {
	public:
		template <typename T>
		void set(const std::string& key, T value) { values[key] = std::move(value); }

		template <typename T>
		T get(const std::string& key, T fallback = T{}) const {
			const auto found = values.find(key);
			if (found == values.end()) return fallback;
			const T* value = std::any_cast<T>(&found->second);
			return value ? *value : fallback;
		}

		bool has(const std::string& key) const { return values.count(key) > 0; }
		void erase(const std::string& key) { values.erase(key); }

	private:
		std::unordered_map<std::string, std::any> values;
	};

	//What a node gets when it's ticked
	struct Context {
		ecs::Registry& registry;
		ecs::Entity self; //the entity the tree runs for
		Blackboard& blackboard;
		float dt;
	};

	class Node {
	public:
		virtual ~Node() = default;

		//runs the node for one frame: onInitialize when it starts, onTerminate when it stops running
		Status tick(Context& context);
		//stops a running node (onTerminate(Aborted)) and forgets its progress
		virtual void abort();
		virtual void reset() { status = Status::Invalid; }
		Status lastStatus() const { return status; }

	protected:
		virtual void onInitialize(Context&) {}
		virtual Status update(Context& context) = 0;
		virtual void onTerminate(Context*, Status) {}

	private:
		Status status = Status::Invalid;
	};

	using NodePtr = std::unique_ptr<Node>;

	class Composite : public Node {
	public:
		Composite& add(NodePtr child) {
			children.push_back(std::move(child));
			return *this;
		}
		void abort() override;
		void reset() override;

	protected:
		//resets the child that was running last frame if a different one runs now, so an interrupted branch
		//starts over instead of resuming halfway through
		void switchRunningChild(size_t index);
		void clearRunningChild();

		std::vector<NodePtr> children;
		size_t runningChild = NO_CHILD;
		static constexpr size_t NO_CHILD = static_cast<size_t>(-1);
	};

	//Tries children in order until one doesn't fail (priority list: "if this, else that")
	class Selector final : public Composite {
	protected:
		Status update(Context& context) override;
	};

	//Runs children in order and fails as soon as one fails. Re-checks every child from the first each frame,
	//so a condition at the front can cut a running action short.
	class Sequence final : public Composite {
	protected:
		Status update(Context& context) override;
	};

	//A sequence that remembers how far it got: children that already succeeded aren't ticked again until the
	//whole sequence finishes or is aborted. Use it for committed routines (wind up, strike, recover).
	class StatefulSequence final : public Composite {
	public:
		void reset() override;
		size_t currentChildIndex() const { return current; }

	protected:
		void onInitialize(Context&) override { current = 0; }
		Status update(Context& context) override;

	private:
		size_t current = 0;
	};

	//Ticks its child forever: whenever the child finishes it's reset and runs again
	class Loop final : public Node {
	public:
		explicit Loop(NodePtr child) : child{ std::move(child) } {}
		void abort() override { child->abort(); Node::abort(); }
		void reset() override { child->reset(); Node::reset(); }

	protected:
		Status update(Context& context) override;

	private:
		NodePtr child;
	};

	//Swaps Success and Failure
	class Inverter final : public Node {
	public:
		explicit Inverter(NodePtr child) : child{ std::move(child) } {}
		void abort() override { child->abort(); Node::abort(); }
		void reset() override { child->reset(); Node::reset(); }

	protected:
		Status update(Context& context) override;

	private:
		NodePtr child;
	};

	//Leaf: Success if the predicate holds, Failure otherwise
	class Condition final : public Node {
	public:
		explicit Condition(std::function<bool(Context&)> predicate) : predicate{ std::move(predicate) } {}

	protected:
		Status update(Context& context) override { return predicate(context) ? Status::Success : Status::Failure; }

	private:
		std::function<bool(Context&)> predicate;
	};

	//Leaf: runs a function every tick and returns whatever it returns
	class Action final : public Node {
	public:
		explicit Action(std::function<Status(Context&)> run) : run{ std::move(run) } {}

	protected:
		Status update(Context& context) override { return run(context); }

	private:
		std::function<Status(Context&)> run;
	};

	//Leaf: Running for `seconds` of game time, then Success
	class Wait final : public Node {
	public:
		explicit Wait(float seconds) : duration{ seconds } {}

	protected:
		void onInitialize(Context&) override { elapsed = 0.f; }
		Status update(Context& context) override;

	private:
		float duration;
		float elapsed = 0.f;
	};

	//helpers for building trees in code
	template <typename... Children>
	NodePtr selector(Children&&... children) {
		auto node = std::make_unique<Selector>();
		(node->add(std::forward<Children>(children)), ...);
		return node;
	}
	template <typename... Children>
	NodePtr sequence(Children&&... children) {
		auto node = std::make_unique<Sequence>();
		(node->add(std::forward<Children>(children)), ...);
		return node;
	}
	template <typename... Children>
	NodePtr statefulSequence(Children&&... children) {
		auto node = std::make_unique<StatefulSequence>();
		(node->add(std::forward<Children>(children)), ...);
		return node;
	}
	inline NodePtr loop(NodePtr child) { return std::make_unique<Loop>(std::move(child)); }
	inline NodePtr inverter(NodePtr child) { return std::make_unique<Inverter>(std::move(child)); }
	inline NodePtr condition(std::function<bool(Context&)> predicate) { return std::make_unique<Condition>(std::move(predicate)); }
	inline NodePtr action(std::function<Status(Context&)> run) { return std::make_unique<Action>(std::move(run)); }
	inline NodePtr wait(float seconds) { return std::make_unique<Wait>(seconds); }
}

namespace nrd {
	//Component: the entity's behaviour tree and its memory
	struct BehaviorTree {
		ai::NodePtr root;
		ai::Blackboard blackboard;
		ai::Status lastStatus = ai::Status::Invalid;
	};

	//ticks every BehaviorTree; the engine runs it each frame
	void updateBehaviorTrees(ecs::Registry& registry, float dt);
}
