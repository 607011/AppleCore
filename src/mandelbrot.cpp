#include <atomic>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include <SFML/Graphics.hpp>
#include <gmpxx.h>
#include <yaml-cpp/yaml.h>

#include "util.hpp"

using palette_t = std::vector<sf::Color>;

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
mpf_class c_real(-0.75, 64);
mpf_class c_imag(0.0, 64);
mp_bitcnt_t min_precision_bits = 64;
unsigned long long base_iterations = 1000;
double log_scale_factor = 0.1;
palette_t palette;
unsigned long long max_iterations_limit = 1'500'000'000ULL;
std::string out_file = "mandelbrot.png";
const char* WINDOW_NAME = "AppleCore";
YAML::Node config;

unsigned long long mandelbrot(mpf_class const& x0, mpf_class const& y0, const unsigned long long max_iterations)
{
    mpf_class x = 0;
    mpf_class y = 0;
    mpf_class x2 = 0;
    mpf_class y2 = 0;
    unsigned long long iterations = 0ULL;
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

unsigned long long calculate_max_iterations(double zoom_level)
{
    unsigned long long max_iterations =
        static_cast<unsigned long long>(base_iterations * std::exp(log_scale_factor * zoom_level));
    return max_iterations;
}

std::string replace_substring(const std::string& str, const std::string& substring, int value, int padding = 6)
{
    std::string result = str;
    std::size_t pos = result.find(substring);
    while (pos != std::string::npos)
    {
        std::stringstream ss;
        ss << std::setw(padding) << std::setfill('0') << value;
        result.replace(pos, substring.length(), ss.str());
        pos = result.find(substring, pos + static_cast<std::size_t>(padding));
    }
    return result;
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
        max_iterations_limit = config["max_iterations_limit"].as<unsigned long long>();
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
        c_real = config["center"]["r"].as<std::string>();
        c_imag = config["center"]["i"].as<std::string>();
    }
    if (config["min_precision_bits"])
    {
        min_precision_bits = config["min_precision_bits"].as<mp_bitcnt_t>();
    }
    if (config["base_iterations"])
    {
        base_iterations = config["base_iterations"].as<unsigned long long>();
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
                palette.emplace_back(sf::Color(rgb.at(0), rgb.at(1), rgb.at(2)));
            }
        }
    }
    if (config["out_file"])
    {
        out_file = config["out_file"].as<std::string>();
    }
}

static std::atomic<int> completed_rows = 0;

void calculate_mandelbrot_row_range(sf::Image& image, mpf_class const& scale_factor, mpf_class const& real_start,
                                    mpf_class const& imag_start, int start_row, int end_row,
                                    const unsigned long long max_iterations)
{
    for (int y = start_row; y < end_row; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            mpf_class const& pixel_real = real_start + x * scale_factor;
            mpf_class const& pixel_imag = imag_start + y * scale_factor;
            const unsigned long long iterations = mandelbrot(pixel_real, pixel_imag, max_iterations);
            const double hue = static_cast<double>(iterations) / static_cast<double>(max_iterations);
            image.setPixel(x, y - start_row, (iterations < max_iterations) ? get_rainbow_color(hue) : sf::Color::Black);
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
    for (unsigned int i = 0; i < n; ++i)
    {
        result_image.copy(partial_images.at(i), 0, i * single_image_height, sf::IntRect(0, 0, width, single_image_height));
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
        std::cerr << "Configuration error: image height (" << height << ") must be divisible by number of threads (" << num_threads << ")." << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Generating " << width << 'x' << height << " image in " << num_threads << " threads. ";
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    std::cout << "Zooming from " << zoom_from << " to " << zoom_to << '.' << std::endl;
    mpf_set_default_prec(min_precision_bits);
    double zoom_level = zoom_from;
    std::vector<sf::Image> images;
    for (int i = 0; i < num_threads; ++i)
    {
        sf::Image image;
        int start_row = i * height / num_threads;
        int end_row = (i + 1) * height / num_threads;
        image.create(width, end_row - start_row, sf::Color::Transparent);
        images.push_back(image);
    }
    std::time_t t0 = std::time(nullptr);
#ifndef HEADLESS
    sf::RenderWindow window(sf::VideoMode(width, height), "AppleCore");
    bool quit_on_next_frame = false;
    while (zoom_level <= zoom_to && window.isOpen() && !quit_on_next_frame)
#else
    while (zoom_level <= zoom_to)
#endif
    {
        mpf_class scale_factor = 4.0 / std::pow(2.0, zoom_level) / std::max(width, height);
        mpf_class real_start = c_real - width / 2.0 * scale_factor;
        mpf_class imag_start = c_imag - height / 2.0 * scale_factor;
        const unsigned long long max_iterations = std::min(max_iterations_limit, calculate_max_iterations(zoom_level));
        std::vector<std::thread> threads;
        std::cout << "Zoom: " << std::scientific << std::setprecision(24) << 1.0 / scale_factor.get_d()
                  << "; max. iterations: " << max_iterations << std::endl;
        completed_rows = 0;
        sf::Image image;
        image.create(width, height, sf::Color::Transparent);
        for (int i = 0; i < num_threads; ++i)
        {
            int start_row = i * height / num_threads;
            int end_row = (i + 1) * height / num_threads;
            threads.emplace_back(calculate_mandelbrot_row_range, std::ref(images[i]), scale_factor, real_start,
                                 imag_start, start_row, end_row, max_iterations);
        }
#ifndef HEADLESS
        while (completed_rows < height && window.isOpen())
        {
            int last_completed_rows = completed_rows;
            while (completed_rows <= last_completed_rows && window.isOpen())
            {
                sf::sleep(sf::milliseconds(100));
            }
            std::cout << "\r" << completed_rows << " (" << std::fixed << std::setprecision(1)
                      << (100.0 * completed_rows / height) << "%)\x1b[K" << std::flush;

            sf::Event event;
            while (window.pollEvent(event))
            {
                switch (event.type)
                {
                case sf::Event::Closed:
                    window.close();
                    break;
                case sf::Event::KeyPressed:
                    if (event.key.code == sf::Keyboard::Q)
                        quit_on_next_frame = true;
                    break;
                default:
                    break;
                }
            }

            sf::Texture texture;
            image = stitch_images(images, height);
            texture.loadFromImage(image);
            sf::Sprite sprite(texture);

            window.draw(sprite);
            window.display();
        }
#endif
        for (std::thread& thread : threads)
        {
            thread.join();
        }
        std::string png_file = replace_substring(out_file, "%z", file_index);
        std::cout << "\rWriting image to " << png_file << "\x1b[K" << std::endl;
        image.saveToFile(png_file);

        ++file_index;
        zoom_level = zoom_level * zoom_factor + zoom_increment;

        std::time_t now = std::time(nullptr);
        auto dt = now - t0;
        config["zoom"]["from"] = zoom_level;
        config["checkpoint"]["file_index"] = file_index;
        config["checkpoint"]["zoom"] = 1.0 / scale_factor.get_d();
        config["checkpoint"]["t0"] = get_iso_timestamp(t0);
        config["checkpoint"]["now"] = get_iso_timestamp(now);
        config["checkpoint"]["elapsed_secs"] = dt;
        config["checkpoint"]["zoom"] = 1.0 / scale_factor.get_d();
        std::ofstream checkpoint("checkpoint.yaml", std::ios::trunc);
        checkpoint << config;
    }

    return EXIT_SUCCESS;
}
