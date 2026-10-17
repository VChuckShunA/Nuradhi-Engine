#pragma once

//std
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lve {
	class LveWindow;
}

namespace nrd {

	enum class Key {
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
		Space, Enter, Escape, Tab, Backspace,
		Left, Right, Up, Down,
		LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		Count
	};

	//Keyboard state, read once per frame by the engine. Game code asks for keys, or better, for named actions and
	//axes, so controls can be rebound in one place:
	//
	//    input.bindAction("jump", Key::Space);      input.actionPressed("jump")
	//    input.bindAxis("moveX", Key::A, Key::D);   input.axis("moveX")  -> -1, 0 or 1
	class Input {
	public:
		explicit Input(lve::LveWindow& window);

		//the engine calls this every frame, after polling window events
		void update();

		bool isDown(Key key) const { return current[index(key)]; }
		bool wasPressed(Key key) const { return current[index(key)] && !previous[index(key)]; } //went down this frame
		bool wasReleased(Key key) const { return !current[index(key)] && previous[index(key)]; } //came up this frame

		//an action can have several keys; it's down if any of them is
		void bindAction(const std::string& action, Key key);
		bool actionDown(const std::string& action) const;
		bool actionPressed(const std::string& action) const;
		bool actionReleased(const std::string& action) const;

		//an axis reads -1 while its negative key is down, +1 for the positive one, 0 for neither or both.
		//Several key pairs can feed the same axis.
		void bindAxis(const std::string& axis, Key negative, Key positive);
		float axis(const std::string& axis) const;

	private:
		static constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(Key::Count);
		static std::size_t index(Key key) { return static_cast<std::size_t>(key); }

		lve::LveWindow& window;
		std::array<bool, KEY_COUNT> current{};
		std::array<bool, KEY_COUNT> previous{};
		std::unordered_map<std::string, std::vector<Key>> actions;
		std::unordered_map<std::string, std::vector<std::pair<Key, Key>>> axes;
	};
}
