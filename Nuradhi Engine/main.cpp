#include "first_app.hpp"
#include "nrd_test_level.hpp"
//std
#include <cstdlib>
#include<iostream>
#include<stdexcept>

int main() {
	//lve::FirstApp app{};
	nrd::TestLevel testLevel{};
	try {
		//app.run();
		testLevel.run();
	}
	catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}