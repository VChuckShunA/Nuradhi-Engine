#include "nrd_globals.hpp"

const float nrd::NrdGlobals::PI = 3.14159;
bool nrd::NrdGlobals::debugging = false; //true prints every collision the player resolves (thousands of lines a second while standing on a floor)
std::string nrd::NrdGlobals::clipOffDataHeader(std::string data)
{
    return std::string();
}
