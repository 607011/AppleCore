#include <atomic>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <boost/multiprecision/mpfr.hpp>
#include <yaml-cpp/yaml.h>

#include "util.hpp"

namespace mp = boost::multiprecision;
using palette_t = std::vector<sf::Color>;
using iteration_count_t = uint64_t;

class thsds_numpunct : public std::numpunct<char>
{
  protected:
    virtual char do_thousands_sep() const
    {
        return ',';
    }
    virtual std::string do_grouping() const
    {
        return "\03";
    }
};

int width = 3840;
int height = 2160;
double zoom_from = 0.25;
double zoom_to = 1000;
double zoom_factor = 1.0;
double zoom_increment = 0.12;
int file_index = 0;
mp::mpfr_float c_real(-0.75, 2048);
mp::mpfr_float c_imag(0.0, 2048);
mpfr_prec_t min_precision_bits = 64;
iteration_count_t base_iterations = 1000;
double log_scale_factor = 0.1;
palette_t palette;
iteration_count_t max_iterations_limit = 2'000'000'000ULL;
std::string out_file = "mandelbrot.png";
std::string checkpoint_file = "checkpoint.yaml";
const char* WINDOW_NAME = "AppleCore";
YAML::Node config;

struct thread_param
{
    sf::Image& image;
    double scale_factor;
    mp::mpfr_float const& real_start;
    mp::mpfr_float const& imag_start;
    int start_row;
    int end_row;
    const unsigned long long max_iterations;
};

iteration_count_t mandelbrot(mp::mpfr_float const& x0, mp::mpfr_float const& y0, const iteration_count_t max_iterations)
{
    mp::mpfr_float x = 0;
    mp::mpfr_float y = 0;
    mp::mpfr_float x2 = 0;
    mp::mpfr_float y2 = 0;
    iteration_count_t iterations = 0ULL;
    while (x2 + y2 <= 4 && iterations < max_iterations)
    {
        y = 2 * x * y + y0;
        x = x2 - y2 + x0;
        x2 = x * x;
        y2 = y * y;
        ++iterations;
    }
    return iterations;
}

iteration_count_t calculate_max_iterations(double zoom_level)
{
    iteration_count_t max_iterations =
        static_cast<iteration_count_t>(base_iterations * std::exp(log_scale_factor * zoom_level));
    return max_iterations;
}

void parse_config_file(std::string const& config_file)
{
    config = YAML::LoadFile(config_file);
    if (config["width"] && config["height"])
    {
        width = config["width"].as<int>();
        height = config["height"].as<int>();
    }
    if (config["max_iterations_limit"])
    {
        max_iterations_limit = config["max_iterations_limit"].as<iteration_count_t>();
    }
    if (config["checkpoint"]["file_index"])
    {
        file_index = config["checkpoint"]["file_index"].as<int>();
    }
    if (config["zoom"]["from"] && config["zoom"]["to"] && config["zoom"]["factor"])
    {
        zoom_from = config["zoom"]["from"].as<double>();
        zoom_to = config["zoom"]["to"].as<double>();
        zoom_factor = config["zoom"]["factor"].as<double>();
        zoom_increment = config["zoom"]["increment"].as<double>();
    }
    if (config["center"]["r"] && config["center"]["i"])
    {
        c_real.assign(config["center"]["r"].as<std::string>());
        c_imag.assign(config["center"]["i"].as<std::string>());
    }
    if (config["min_precision_bits"])
    {
        min_precision_bits = config["min_precision_bits"].as<mpfr_prec_t>();
    }
    if (config["base_iterations"])
    {
        base_iterations = config["base_iterations"].as<iteration_count_t>();
    }
    if (config["log_scale_factor"])
    {
        log_scale_factor = config["log_scale_factor"].as<double>();
    }
    if (config["palette"] || config["palette"].IsSequence())
    {
        auto parse_rgb = [](std::string const& str) {
            std::vector<sf::Uint8> numbers;
            std::stringstream ss(str);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                int number;
                std::stringstream token_stream(token);
                token_stream >> number;
                numbers.push_back(static_cast<sf::Uint8>(number));
            }
            return numbers;
        };
        palette.clear();
        for (auto it : config["palette"])
        {
            std::vector<sf::Uint8> const& rgb = parse_rgb(it.as<std::string>());
            if (rgb.size() == 3)
            {
                palette.emplace_back(rgb.at(0), rgb.at(1), rgb.at(2));
            }
        }
    }
    if (config["out_file"])
    {
        out_file = config["out_file"].as<std::string>();
    }
    if (config["checkpoint_file"])
    {
        checkpoint_file = config["checkpoint_file"].as<std::string>();
    }
}

static std::atomic<int> completed_rows = 0;

void calculate_mandelbrot_row_range(thread_param const& p)
{
    for (int y = p.start_row; y < p.end_row; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            mp::mpfr_float const& pixel_real = p.real_start + p.scale_factor * x;
            mp::mpfr_float const& pixel_imag = p.imag_start + p.scale_factor * y;
            const unsigned long long iterations = mandelbrot(pixel_real, pixel_imag, p.max_iterations);
            const double hue = static_cast<double>(iterations) / static_cast<double>(p.max_iterations);
            p.image.setPixel(x, y - p.start_row,
                             (iterations < p.max_iterations) ? get_rainbow_color(hue) : sf::Color::Black);
        }
        ++completed_rows;
    }
}

sf::Image stitch_images(std::vector<sf::Image> const& partial_images, int height)
{
    int width = static_cast<int>(partial_images.front().getSize().x);
    int n = static_cast<int>(partial_images.size());
    int single_image_height = height / n;
    sf::Image result_image;
    result_image.create(width, height);
    for (int i = 0; i < n; ++i)
    {
        result_image.copy(partial_images.at(i), 0, i * single_image_height,
                          sf::IntRect(0, 0, width, single_image_height));
    }
    return result_image;
}

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        parse_config_file(argv[1]);
    }
    int num_threads = static_cast<int>(std::thread::hardware_concurrency());
    if (height % num_threads != 0)
    {
        std::cerr << "Configuration error: image height (" << height << ") must be divisible by number of threads ("
                  << num_threads << ")." << std::endl;
        return EXIT_FAILURE;
    }
    time_t t0 = std::time(nullptr);
    std::cout << "Generating " << width << 'x' << height << " image in " << num_threads << " threads. ";
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    std::cout << "Zooming from " << zoom_from << " to " << zoom_to << '.' << std::endl;
    mpfr_set_default_prec(min_precision_bits);
    std::vector<sf::Image> images;
    for (int i = 0; i < num_threads; ++i)
    {
        sf::Image image;
        int start_row = i * height / num_threads;
        int end_row = (i + 1) * height / num_threads;
        image.create(width, end_row - start_row, sf::Color::Transparent);
        images.push_back(image);
    }
    double zoom_level = zoom_from;
#ifndef HEADLESS
    sf::RenderWindow window(sf::VideoMode(width, height), "AppleCore");
    sf::Event event;
    window.clear(sf::Color::Green);
    window.display();
    (void)window.pollEvent(event);
    bool quit_on_next_frame = false;
    while (zoom_level <= zoom_to && window.isOpen() && !quit_on_next_frame)
#else
    while (zoom_level <= zoom_to)
#endif
    {
        double scale_factor = 4.0 / std::pow(2.0, zoom_level) / std::max(width, height);
        mp::mpfr_float real_start = c_real - width / 2.0 * scale_factor;
        mp::mpfr_float imag_start = c_imag - height / 2.0 * scale_factor;
        const unsigned long long max_iterations = std::min(max_iterations_limit, calculate_max_iterations(zoom_level));
        std::vector<std::thread> threads;
        std::cout << "Zoom: " << std::setprecision(6) << std::defaultfloat << zoom_level
                  << "; Δpixel: " << std::scientific << std::setprecision(24) << scale_factor
                  << "; max. iterations: " << max_iterations << std::endl;
        completed_rows = 0;
        sf::Image image;
        image.create(width, height, sf::Color::Transparent);
        for (int i = 0; i < num_threads; ++i)
        {
            int start_row = i * height / num_threads;
            int end_row = (i + 1) * height / num_threads;
            threads.emplace_back(calculate_mandelbrot_row_range, thread_param{.image = images[i],
                                                                              .scale_factor = scale_factor,
                                                                              .real_start = real_start,
                                                                              .imag_start = imag_start,
                                                                              .start_row = start_row,
                                                                              .end_row = end_row,
                                                                              .max_iterations = max_iterations});
        }
#ifndef HEADLESS
        sf::Vector2i last_mouse_pos = sf::Mouse::getPosition(window);
        while (completed_rows < height && window.isOpen())
        {
            int last_completed_rows = completed_rows;
            while (completed_rows <= last_completed_rows && window.isOpen() &&
                   last_mouse_pos == sf::Mouse::getPosition(window))
            {
                sf::sleep(sf::milliseconds(100));
            }
            last_mouse_pos = sf::Mouse::getPosition(window);
            std::cout << "\r" << completed_rows << " (" << std::fixed << std::setprecision(1)
                      << (100.0 * completed_rows / height) << "%)\x1b[K" << std::flush;

            sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);
            std::ostringstream coords_ss;
            mp::mpfr_float const& pixel_real = real_start + mouse_pos.x * scale_factor;
            mp::mpfr_float const& pixel_imag = imag_start + mouse_pos.y * scale_factor;
            coords_ss << "r: " << pixel_real << "\n" << "i: " << pixel_imag;

            while (window.pollEvent(event))
            {
                switch (event.type)
                {
                case sf::Event::Closed:
                    window.close();
                    break;
                case sf::Event::KeyPressed:
                    if ((event.key.system || event.key.control) && event.key.code == sf::Keyboard::C)
                    {
                        sf::Clipboard::setString(coords_ss.str());
                    }
                    else if (event.key.code == sf::Keyboard::Q)
                    {
                        quit_on_next_frame = true;
                    }
                    break;
                default:
                    break;
                }
            }

            sf::Texture texture;
            image = stitch_images(images, height);
            texture.loadFromImage(image);
            texture.setSmooth(false);
            sf::Sprite sprite(texture);

            window.clear();
            window.draw(sprite);
            window.display();
        }
#endif
        for (std::thread& thread : threads)
        {
            thread.join();
        }
        std::string fidx = std::to_string(file_index);
        fidx = std::string(6U - fidx.length(), '0') + fidx;
        std::string png_file = replace_substring(out_file, "{file_index}", fidx);
        png_file = replace_substring(png_file, "{max_iterations}", std::to_string(max_iterations));
        png_file = replace_substring(png_file, "{log_scale_factor}", std::to_string(log_scale_factor));
        png_file = replace_substring(png_file, "{zoom_level}", std::to_string(zoom_level));
        png_file = replace_substring(png_file, "{size}", std::to_string(width) + 'x' + std::to_string(height));
        std::cout << "\rWriting image to " << png_file << "\x1b[K" << std::endl;
        image.saveToFile(png_file);

        ++file_index;
        zoom_level = zoom_level * zoom_factor + zoom_increment;

        std::time_t now = std::time(nullptr);
        auto dt = now - t0;
        config["zoom"]["from"] = zoom_level;
        config["checkpoint"]["file_index"] = file_index;
        config["checkpoint"]["zoom"] = 1.0 / scale_factor;
        config["checkpoint"]["t0"] = get_iso_timestamp(t0);
        config["checkpoint"]["now"] = get_iso_timestamp(now);
        config["checkpoint"]["elapsed_secs"] = dt;
        std::ofstream checkpoint(checkpoint_file, std::ios::trunc);
        checkpoint << config;
    }

    return EXIT_SUCCESS;
}
