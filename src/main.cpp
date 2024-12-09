#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <boost/multiprecision/mpfr.hpp>
#include <yaml-cpp/yaml.h>

#include "1000s.hpp"
#include "mandelbrot.hpp"
#include "util.hpp"

namespace mp = boost::multiprecision;
namespace chrono = std::chrono;
// using FloatType = mp::mpfr_float;
using FloatType = double;
using mandelbrot_computer_t = mandelbrot_calculator<FloatType>;
// using mandelbrot_computer_t = mandelbrot_calculator_perturbative<FloatType>;
using palette_t = std::vector<sf::Color>;

int num_threads = static_cast<int>(std::thread::hardware_concurrency());
double zoom_from = 0.25;
double zoom_to = 1000;
double zoom_factor = 1.0;
double zoom_increment = 0.12;
int file_index = 0;
FloatType c_real = -0.75;
FloatType c_imag = 0.0;
mpfr_prec_t min_precision_bits = 64;
double log_scale_factor = 0.1;
palette_t palette;
std::string out_file = "mandelbrot.png";
std::string checkpoint_file = "checkpoint.yaml";
YAML::Node config;

void parse_config_file(std::string const& config_file, mandelbrot_computer_t& mandelbrot)
{
    config = YAML::LoadFile(config_file);
    if (config["width"] && config["height"])
    {
        mandelbrot.width = config["width"].as<int>();
        mandelbrot.height = config["height"].as<int>();
    }
    if (config["max_iterations_limit"])
    {
        mandelbrot.max_iterations_limit = config["max_iterations_limit"].as<iteration_count_t>();
    }
    if (config["base_iterations"])
    {
        mandelbrot.base_iterations = config["base_iterations"].as<iteration_count_t>();
    }
    if (config["log_scale_factor"])
    {
        mandelbrot.log_scale_factor = config["log_scale_factor"].as<double>();
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
        c_real = config["center"]["r"].as<FloatType>();
        c_imag = config["center"]["i"].as<FloatType>();
        // else
        // {
        //     c_real.assign(config["center"]["r"].as<std::string>());
        //     c_imag.assign(config["center"]["i"].as<std::string>());
        // }
    }
    if (config["min_precision_bits"])
    {
        min_precision_bits = config["min_precision_bits"].as<mpfr_prec_t>();
    }
    if (config["num_threads"])
    {
        num_threads = config["num_threads"].as<int>();
    }
    if (config["palette"] || config["palette"].IsSequence())
    {
        auto parse_rgb = [](std::string const& str) -> std::vector<sf::Uint8> {
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

sf::Image stitch_images(std::vector<sf::Image> const& partial_images, int h)
{
    const int w = static_cast<int>(partial_images.front().getSize().x);
    const int n = static_cast<int>(partial_images.size());
    int single_image_height = h / n;
    sf::Image result_image;
    result_image.create(w, h);
    for (int i = 0; i < n; ++i)
    {
        result_image.copy(partial_images.at(i), 0, i * single_image_height, sf::IntRect(0, 0, w, single_image_height));
    }
    return result_image;
}

int main(int argc, char* argv[])
{
    mandelbrot_computer_t mandelbrot;
    if (argc > 1)
    {
        parse_config_file(argv[1], mandelbrot);
    }
    static constexpr float view_scale = 0.25;
    mpfr_set_default_prec(min_precision_bits);
    if (mandelbrot.height % num_threads != 0)
    {
        std::cerr << "Configuration error: image height (" << mandelbrot.height
                  << ") must be divisible by number of threads (" << num_threads << ")." << std::endl;
        return EXIT_FAILURE;
    }
    auto t0 = chrono::system_clock::now();
    std::cout << "Generating " << mandelbrot.width << 'x' << mandelbrot.height << " image in " << num_threads
              << " threads. ";
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    std::cout << "Zooming from " << zoom_from << " to " << zoom_to << '.' << std::endl;

    // Create full image and partial images
    sf::Image full_image;
    full_image.create(mandelbrot.width, mandelbrot.height, sf::Color::Transparent);
    std::vector<sf::Image> partial_images(num_threads);
    for (int i = 0; i < num_threads; ++i)
    {
        int start_row = i * mandelbrot.height / num_threads;
        int end_row = (i + 1) * mandelbrot.height / num_threads;
        partial_images[i].create(mandelbrot.width, end_row - start_row, sf::Color::Transparent);
    }

    // Zoom in
#ifndef HEADLESS
    std::vector<sf::Texture> textures(partial_images.size());
    std::vector<sf::Sprite> sprites(partial_images.size());
    sf::RenderWindow window(sf::VideoMode(mandelbrot.width / 4, mandelbrot.height / 4), "AppleCore");
    sf::Event event;
    window.clear(sf::Color::Green);
    window.display();
    (void)window.pollEvent(event);
    bool quit_on_next_frame = false;
    double zoom_level = zoom_from;
    while (zoom_level <= zoom_to && window.isOpen() && !quit_on_next_frame)
#else
    double zoom_level = zoom_from;
    while (zoom_level <= zoom_to)
#endif
    {
        const double scale_factor = 4.0 / std::pow(2.0, zoom_level) / std::max(mandelbrot.width, mandelbrot.height);
        FloatType real_start = c_real - mandelbrot.width / 2.0 * scale_factor;
        FloatType imag_start = c_imag - mandelbrot.height / 2.0 * scale_factor;
        mandelbrot.completed_rows = 0;
        const iteration_count_t max_iterations =
            std::min(mandelbrot.max_iterations_limit, mandelbrot.calculate_max_iterations(zoom_level));
        std::cout << "\rZoom: " << std::setprecision(6) << std::defaultfloat << zoom_level
                  << "; Δpixel: " << std::scientific << std::setprecision(24) << scale_factor
                  << "; max. iterations: " << max_iterations << "\x1b[K" << std::flush;

        auto frame_t0 = chrono::system_clock::now();

        // launch Mandelbrot calculator threads
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i)
        {
            const int start_row = i * mandelbrot.height / num_threads;
            const int end_row = (i + 1) * mandelbrot.height / num_threads;
            threads.emplace_back(&mandelbrot_computer_t::calculate_mandelbrot_row_range, &mandelbrot,
                                 mandelbrot_computer_t::thread_param{.image = partial_images[i],
                                                                     .scale_factor = scale_factor,
                                                                     .real_start = real_start,
                                                                     .imag_start = imag_start,
                                                                     .start_row = start_row,
                                                                     .end_row = end_row,
                                                                     .max_iterations = max_iterations});
        }

#ifndef HEADLESS
        sf::Vector2i last_mouse_pos = sf::Mouse::getPosition(window);
        while (mandelbrot.completed_rows < mandelbrot.height && window.isOpen())
        {
            int last_completed_rows = mandelbrot.completed_rows;
            while (mandelbrot.completed_rows <= last_completed_rows && window.isOpen() &&
                   last_mouse_pos == sf::Mouse::getPosition(window))
            {
                sf::sleep(sf::milliseconds(100));
            }
            last_mouse_pos = sf::Mouse::getPosition(window);
            std::cout << "\r" << mandelbrot.completed_rows << " of " << mandelbrot.height << " rows completed (" << std::fixed
                      << std::setprecision(1) << (100.0 * mandelbrot.completed_rows / mandelbrot.height) << "%)\x1b[K"
                      << std::flush;
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
                        sf::Vector2i const& mouse_pos = sf::Mouse::getPosition(window);
                        std::ostringstream coords_ss;
                        FloatType const& pixel_real = real_start + mouse_pos.x * scale_factor;
                        FloatType const& pixel_imag = imag_start + mouse_pos.y * scale_factor;
                        coords_ss << "r: " << pixel_real << "\n" << "i: " << pixel_imag;
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
            window.clear();
            for (size_t i = 0; i < partial_images.size(); ++i)
            {
                textures[i].loadFromImage(partial_images.at(i));
                sprites[i].setTexture(textures.at(i));
                const int start_row = i * mandelbrot.height / num_threads;
                sprites[i].setPosition(0, start_row / 4);
                sprites[i].setScale(0.25, 0.25);
                window.draw(sprites.at(i));
            }
            window.display();
        }
#endif
        for (std::thread& thread : threads)
        {
            thread.join();
        }
        std::cout << "\rStitching final image ... \x1b[K" << std::flush;
        full_image = stitch_images(partial_images, mandelbrot.height);
        std::string fidx = std::to_string(file_index);
        fidx = std::string(6U - fidx.length(), '0') + fidx;
        std::string png_file = replace_substring(out_file, "{file_index}", fidx);
        png_file = replace_substring(png_file, "{max_iterations}", std::to_string(max_iterations));
        png_file = replace_substring(png_file, "{log_scale_factor}", std::to_string(log_scale_factor));
        png_file = replace_substring(png_file, "{zoom_level}", std::to_string(zoom_level));
        png_file = replace_substring(png_file, "{size}",
                                     std::to_string(mandelbrot.width) + 'x' + std::to_string(mandelbrot.height));
        std::cout << "\rWriting image to " << png_file << "\x1b[K" << std::flush;
        full_image.saveToFile(png_file);
        auto now = chrono::system_clock::now();
        std::cout << "\rElapsed time: " << format_duration(now - frame_t0) << "\x1b[K" << std::flush;

        ++file_index;
        zoom_level = zoom_level * zoom_factor + zoom_increment;

        config["zoom"]["from"] = zoom_level;
        config["checkpoint"]["file_index"] = file_index;
        config["checkpoint"]["zoom"] = 1.0 / scale_factor;
        config["checkpoint"]["t0"] = get_iso_timestamp(t0);
        config["checkpoint"]["now"] = get_current_iso_timestamp();
        config["checkpoint"]["elapsed_total"] = format_duration(now - t0);
        config["checkpoint"]["elapsed_last_frame"] = format_duration(now - frame_t0);
        std::ofstream checkpoint(checkpoint_file, std::ios::trunc);
        checkpoint << config;
    }

    return EXIT_SUCCESS;
}
