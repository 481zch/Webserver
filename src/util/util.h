#include <string>
#include <iostream>

std::pair<std::string, std::string> split(std::string& origin, char delimiter)
{
    auto pos = origin.find(delimiter);
    return {origin.substr(0, pos), origin.substr(pos + 1)};
}