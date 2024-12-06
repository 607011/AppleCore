#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <ctime>
#include <string>
#include <SFML/Graphics.hpp>

extern sf::Color get_rainbow_color(double);
extern std::string get_iso_timestamp(time_t);
extern std::string get_current_iso_timestamp(void);
extern std::string replace_substring(std::string const &str, std::string const &substring, std::string const &value);

#endif // __UTIL_HPP__
