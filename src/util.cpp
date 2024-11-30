#include "util.hpp"

sf::Color get_rainbow_color(double value)
{
    int hue = static_cast<int>(value * 360) % 360;
    int r = 0, g = 0, b = 0;
    int phase = hue / 60;
    int x = (hue % 60) * 255 / 60;
    switch (phase)
    {
    case 0:
        r = 255;
        g = x;
        b = 0;
        break;
    case 1:
        r = 255 - x;
        g = 255;
        b = 0;
        break;
    case 2:
        r = 0;
        g = 255;
        b = x;
        break;
    case 3:
        r = 0;
        g = 255 - x;
        b = 255;
        break;
    case 4:
        r = x;
        g = 0;
        b = 255;
        break;
    case 5:
        r = 255;
        g = 0;
        b = 255 - x;
        break;
    }
    return sf::Color(static_cast<sf::Uint8>(r), static_cast<sf::Uint8>(g), static_cast<sf::Uint8>(b));
}
