#include "nrd_random_number.hpp"

int nrd::NrdRandomNumber::getRandomNumber(int mod)
{
	//returns random number between 0 and mod
	return rand() % mod;
}
