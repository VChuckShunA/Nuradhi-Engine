#pragma once

#include "game/game_components.hpp"
#include "engine/ai/behavior_tree.hpp"
#include "engine/core/engine.hpp"

namespace game {

	//Behaviour tree for an EnemyFighter, highest priority first:
	//
	//  Selector
	//  |- Sequence          stunned or beaten: stand there (hitStunSystem slides any knockback out)
	//  |- Sequence          no target, or it's out of aggro range: idle
	//  |- Sequence          it's this enemy's turn to attack (role Attacker):
	//  |    Selector
	//  |    |- StatefulSequence  target in reach: face it, stop and wind up (the player's warning), swing the planned
	//  |    |                    move, wait for it to finish, recover, then hand the turn back. Once started it's
	//  |    |                    committed, unless a hit interrupts it.
	//  |    |- Action            otherwise walk up to the target and line up with it in depth
	//  |- Action            otherwise (role Waiter): hold its slot around the target, facing it, and keep clear of
	//                       the other enemies
	//
	//The entity needs Transform, RigidBody, Sprite, Combat (with "punch" and "kick" moves), Health and EnemyFighter.
	//Without an EnemySquad every enemy is an Attacker and fights on its own.
	nrd::ai::NodePtr makeFighterTree();

	//Phase::Update, before the trees run each frame. Decides who attacks and who waits (see EnemySquad), assigns the
	//waiters their places around the target, and takes a turn back from an enemy that has finished or been hit.
	//Does nothing without an EnemySquad entity.
	void enemySquadSystem(nrd::Engine& engine, float dt);

	//Picks how the enemy's next attack will go: a punch or a kick, and how close it walks up first. The tree does this
	//after every attack; call it on a new enemy too, so the first one isn't always the default.
	void planNextAttack(EnemyFighter& fighter);
}
