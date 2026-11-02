#pragma once

#include "engine/core/engine.hpp"
#include "engine/render/render_components.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <functional>
#include <string>

namespace game {

	//Component: a prop's model-space bounds (+y down). When the editor rescales the prop, its Collider is rebuilt
	//from these times the new scale, so the hitbox always fits the mesh. Props without it have their collider
	//scaled in proportion instead.
	struct EditorBounds {
		glm::vec3 min{ -0.5f };
		glm::vec3 max{ 0.5f };
	};

	//Things the game lets the editor do, as buttons in its window. Leave one empty and its button isn't shown.
	struct EditorActions {
		//Rebuilds the sprite atlas from the source images and swaps it into the running game; returns a line to show
		//the result (it may throw: the message is shown instead). The game stands still while it runs, a few seconds.
		std::function<std::string()> rebakeSprites;
	};

	//Adds the level editor, toggled with the "toggleEditor" action (F2 in the test level). It lists every entity
	//with a Name; the selected one can be moved, rotated, scaled or removed, and new props can be placed from a
	//few prefabs (their meshes are loaded now and drawn with `propTexture`).
	//
	//Colliders are axis aligned: rotating a prop turns its mesh, not its hitbox. Sprites ignore rotation and scale.
	void addLevelEditor(nrd::Engine& engine, nrd::TextureHandle propTexture, EditorActions actions = {});
}
