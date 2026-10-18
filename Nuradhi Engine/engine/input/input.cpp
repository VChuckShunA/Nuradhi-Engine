#include "engine/input/input.hpp"
#include "engine/platform/lve_window.hpp"

namespace nrd {

	namespace {
		constexpr std::size_t FIRST_MOUSE_BUTTON = static_cast<std::size_t>(Key::MouseLeft);
		constexpr std::array<int, 3> GLFW_MOUSE_BUTTONS{ GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, GLFW_MOUSE_BUTTON_MIDDLE };

		//GLFW key code for each keyboard Key, in enum order
		constexpr std::array<int, FIRST_MOUSE_BUTTON> GLFW_KEYS{
			GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C, GLFW_KEY_D, GLFW_KEY_E, GLFW_KEY_F, GLFW_KEY_G, GLFW_KEY_H, GLFW_KEY_I,
			GLFW_KEY_J, GLFW_KEY_K, GLFW_KEY_L, GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_O, GLFW_KEY_P, GLFW_KEY_Q, GLFW_KEY_R,
			GLFW_KEY_S, GLFW_KEY_T, GLFW_KEY_U, GLFW_KEY_V, GLFW_KEY_W, GLFW_KEY_X, GLFW_KEY_Y, GLFW_KEY_Z,
			GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9,
			GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_TAB, GLFW_KEY_BACKSPACE,
			GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN,
			GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL, GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT,
			GLFW_KEY_F1, GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4, GLFW_KEY_F5, GLFW_KEY_F6,
			GLFW_KEY_F7, GLFW_KEY_F8, GLFW_KEY_F9, GLFW_KEY_F10, GLFW_KEY_F11, GLFW_KEY_F12,
		};
	}

	Input::Input(lve::LveWindow& window) : window{ window }
	{
	}

	void Input::update()
	{
		previous = current;
		GLFWwindow* glfwWindow = window.getGLFWwindow();
		for (std::size_t key = 0; key < FIRST_MOUSE_BUTTON; key++) {
			current[key] = glfwGetKey(glfwWindow, GLFW_KEYS[key]) == GLFW_PRESS;
		}
		for (std::size_t button = 0; button < GLFW_MOUSE_BUTTONS.size(); button++) {
			current[FIRST_MOUSE_BUTTON + button] = glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTONS[button]) == GLFW_PRESS;
		}
	}

	void Input::bindAction(const std::string& action, Key key)
	{
		actions[action].push_back(key);
	}

	bool Input::actionDown(const std::string& action) const
	{
		const auto found = actions.find(action);
		if (found == actions.end()) return false;
		for (Key key : found->second) {
			if (isDown(key)) return true;
		}
		return false;
	}

	bool Input::actionPressed(const std::string& action) const
	{
		const auto found = actions.find(action);
		if (found == actions.end()) return false;
		bool downBefore = false;
		for (Key key : found->second) {
			if (previous[index(key)]) downBefore = true;
		}
		return actionDown(action) && !downBefore;
	}

	bool Input::actionReleased(const std::string& action) const
	{
		const auto found = actions.find(action);
		if (found == actions.end()) return false;
		bool downBefore = false;
		for (Key key : found->second) {
			if (previous[index(key)]) downBefore = true;
		}
		return !actionDown(action) && downBefore;
	}

	void Input::bindAxis(const std::string& axis, Key negative, Key positive)
	{
		axes[axis].emplace_back(negative, positive);
	}

	float Input::axis(const std::string& axis) const
	{
		const auto found = axes.find(axis);
		if (found == axes.end()) return 0.f;
		float value = 0.f;
		for (const auto& [negative, positive] : found->second) {
			if (isDown(negative)) value -= 1.f;
			if (isDown(positive)) value += 1.f;
		}
		return value < -1.f ? -1.f : (value > 1.f ? 1.f : value);
	}
}
