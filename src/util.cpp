#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/serialization/vector.hpp>

#include "util.hpp"

namespace bios = boost::iostreams;

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

std::string get_iso_timestamp(std::chrono::system_clock::time_point const& t)
{
    auto time_t_now =
        std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(t));
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string get_current_iso_timestamp(void)
{
    return get_iso_timestamp(std::chrono::system_clock::now());
}

std::string replace_substring(std::string const& str, const std::string& substring, std::string const& value)
{
    std::string result = str;
    std::size_t pos = result.find(substring);
    while (pos != std::string::npos)
    {
        result.replace(pos, substring.length(), value);
        pos = result.find(substring, pos + value.length());
    }
    return result;
}

void save_result(std::vector<iteration_count_t> const& result_buffer, int width, int height,
                 std::string const& filename)
{
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    bios::filtering_ostreambuf out;
    out.push(bios::zlib_compressor());
    out.push(file);
    boost::archive::binary_oarchive oa(out);
    oa << width << height << result_buffer;
}

std::vector<iteration_count_t> load_result(std::string const& filename, int& width, int& height)
{
    std::ifstream file(filename, std::ios::binary);
    std::vector<iteration_count_t> buf;
    bios::filtering_istreambuf in;
    in.push(bios::zlib_decompressor());
    in.push(file);
    boost::archive::binary_iarchive ia(in);
    ia >> width;
    ia >> height;
    buf.reserve(width * height);
    ia >> buf;
    return buf;
}
