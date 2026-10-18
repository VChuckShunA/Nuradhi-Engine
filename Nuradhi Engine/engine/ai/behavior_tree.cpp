#include "engine/ai/behavior_tree.hpp"

namespace nrd::ai {

	Status Node::tick(Context& context)
	{
		if (status != Status::Running) onInitialize(context);
		status = update(context);
		if (status != Status::Running) onTerminate(&context, status);
		return status;
	}

	void Node::abort()
	{
		if (status == Status::Running) onTerminate(nullptr, Status::Aborted);
		reset();
	}

	void Composite::abort()
	{
		if (runningChild != NO_CHILD) children[runningChild]->abort();
		runningChild = NO_CHILD;
		Node::abort();
	}

	void Composite::reset()
	{
		for (NodePtr& child : children) child->reset();
		runningChild = NO_CHILD;
		Node::reset();
	}

	void Composite::switchRunningChild(size_t index)
	{
		if (runningChild != NO_CHILD && runningChild != index) children[runningChild]->abort();
		runningChild = index;
	}

	void Composite::clearRunningChild()
	{
		//a child that was running and wasn't ticked to an end this frame was cut off by an earlier one
		if (runningChild != NO_CHILD && children[runningChild]->lastStatus() == Status::Running) children[runningChild]->abort();
		runningChild = NO_CHILD;
	}

	Status Selector::update(Context& context)
	{
		for (size_t i = 0; i < children.size(); i++) {
			const Status status = children[i]->tick(context);
			if (status == Status::Failure) continue;
			if (status == Status::Running) switchRunningChild(i);
			else clearRunningChild();
			return status;
		}
		clearRunningChild();
		return Status::Failure;
	}

	Status Sequence::update(Context& context)
	{
		for (size_t i = 0; i < children.size(); i++) {
			const Status status = children[i]->tick(context);
			if (status == Status::Success) continue;
			if (status == Status::Running) switchRunningChild(i);
			else clearRunningChild();
			return status;
		}
		clearRunningChild();
		return Status::Success;
	}

	void StatefulSequence::reset()
	{
		current = 0;
		Composite::reset();
	}

	Status StatefulSequence::update(Context& context)
	{
		//children that finish move straight on to the next one in the same frame
		while (current < children.size()) {
			const Status status = children[current]->tick(context);
			if (status == Status::Success) {
				current++;
				continue;
			}
			if (status == Status::Running) {
				switchRunningChild(current);
				return Status::Running;
			}
			runningChild = NO_CHILD;
			current = 0;
			return status;
		}
		runningChild = NO_CHILD;
		current = 0;
		return Status::Success;
	}

	Status Loop::update(Context& context)
	{
		const Status status = child->tick(context);
		if (status == Status::Success || status == Status::Failure) {
			child->reset(); //start over next frame
			return Status::Running;
		}
		return status;
	}

	Status Inverter::update(Context& context)
	{
		const Status status = child->tick(context);
		if (status == Status::Success) return Status::Failure;
		if (status == Status::Failure) return Status::Success;
		return status;
	}

	Status Wait::update(Context& context)
	{
		elapsed += context.dt;
		return elapsed >= duration ? Status::Success : Status::Running;
	}
}

namespace nrd {
	void updateBehaviorTrees(ecs::Registry& registry, float dt)
	{
		registry.each<BehaviorTree>([&](ecs::Entity entity, BehaviorTree& tree) {
			if (!tree.root) return;
			ai::Context context{ registry, entity, tree.blackboard, dt };
			tree.lastStatus = tree.root->tick(context);
		});
	}
}
