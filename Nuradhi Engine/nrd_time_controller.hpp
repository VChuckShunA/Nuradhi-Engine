#pragma once
#include <iostream>
#include <chrono>

namespace nrd {
	class NrdTimeController
	{
	public://reference values
		static const int PLAY_STATE, PAUSE_STATE;

		int timeState;
		float lastUpdate; //Last time we shcekd how many ticks we were up to
		float dT; //delta time, in seconds (1=1seconds,0.5 = half a second). Time since the last frame was rendered onto the screen
		NrdTimeController();
		void updateTime();//update lastUpdate and dT
		void pause();
		void resume();
		void reset();

		static NrdTimeController nrdTimeController;

	};


}