#pragma once

#include "engine/ai/behavior_tree.hpp"

namespace game {

	//Behaviour tree for an EnemyFighter, highest priority first:
	//
	//  Selector
	//  |- Sequence        stunned or beaten: stand there (hitStunSystem slides any knockback out)
	//  |- Sequence        no target, or it's out of aggro range: idle
	//  |- StatefulSequence  target in reach: face it, punch or kick, wait for the attack to finish, then
	//  |                    recover for a moment. Once started it's committed, unless a hit interrupts it.
	//  |- Action          otherwise walk up to the target and line up with it in depth
	//
	//The entity needs Transform, RigidBody, Sprite, Combat (with "punch" and "kick" moves), Health and
	//EnemyFighter.
	nrd::ai::NodePtr makeFighterTree();
}
