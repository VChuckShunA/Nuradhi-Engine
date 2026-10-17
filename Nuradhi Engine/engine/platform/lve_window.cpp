#include "lve_window.hpp"
//std
#include <stdexcept>
namespace lve {
	lve::LveWindow::LveWindow(int w, int h, std::string name) : width{w},height{h},windowName{name}
	{
		initWindow();
	}

	LveWindow::~LveWindow()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Window Surface");
		}
	}

	void LveWindow::frameBufferResizedCallback(GLFWwindow* window, int width, int height)
	{
		auto lveWindow = reinterpret_cast<LveWindow*>(glfwGetWindowUserPointer(window));
		lveWindow->frameBufferResized = true;
		lveWindow->width = width;
		lveWindow->height = height;
	}

	void LveWindow::initWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Do no use Open GL
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);//Resize

		window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr); //3rd parameter to set full screen
		glfwSetWindowUserPointer(window,this);
		glfwSetFramebufferSizeCallback(window, frameBufferResizedCallback);
	}

}