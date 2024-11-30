#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <ctime>
#include <string>
#include <SFML/Graphics.hpp>

extern sf::Color get_rainbow_color(double);
extern std::string get_iso_timestamp(std::time_t);
extern std::string get_current_iso_timestamp(void);

#endif // __UTIL_HPP__
