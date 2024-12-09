#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <SFML/Graphics.hpp>
#include <chrono>
#include <sstream>
#include <string>

extern sf::Color get_rainbow_color(double);
extern std::string get_iso_timestamp(std::chrono::system_clock::time_point const& t);
extern std::string get_current_iso_timestamp(void);
extern std::string replace_substring(std::string const& str, std::string const& substring, std::string const& value);

template <typename Duration> std::string format_duration(Duration dt)
{
    auto days = std::chrono::duration_cast<std::chrono::days>(dt);
    dt -= days;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(dt);
    dt -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(dt);
    dt -= minutes;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(dt);
    std::ostringstream oss;
    bool non_zero_added = false;
    if (days.count() > 0)
    {
        oss << days.count() << " day" << (hours.count() == 1 ? " " : "s ");
        non_zero_added = true;
    }
    if (hours.count() > 0 || non_zero_added)
    {
        oss << hours.count() << " hour" << (hours.count() == 1 ? " " : "s ");
        non_zero_added = true;
    }
    if (minutes.count() > 0 || non_zero_added)
    {
        oss << minutes.count() << " minute" << (minutes.count() == 1 ? " " : "s ");
        non_zero_added = true;
    }
    oss << seconds.count() << " second" << (seconds.count() == 1 ? "" : "s");
    return oss.str();
}

#endif // __UTIL_HPP__
