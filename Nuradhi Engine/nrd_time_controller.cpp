#include "nrd_time_controller.hpp"
const int	nrd::NrdTimeController::PLAY_STATE = 0, 
			nrd::NrdTimeController::PAUSE_STATE = 1;
namespace nrd {
	NrdTimeController::NrdTimeController()
	{
		dT = 0;
		lastUpdate = 0;
		timeState = PLAY_STATE;

	}

	void NrdTimeController::updateTime()
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		if (timeState == PAUSE_STATE)
		{
			dT = 0;
		}
		else {
			auto newTime = std::chrono::high_resolution_clock::now();
			dT = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;
		}

	}

	void NrdTimeController::pause()
	{
		timeState = PAUSE_STATE;
	}

	void NrdTimeController::resume()
	{
		timeState = PLAY_STATE;
	}

	void NrdTimeController::reset()
	{
		dT = 0;
		lastUpdate = 0;// std::chrono::high_resolution_clock::now();
	}


	//builds a global timeController object for use anywhere
	NrdTimeController NrdTimeController::nrdTimeController;

}