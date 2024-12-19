#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <boost/multiprecision/mpfr.hpp>
#include <yaml-cpp/yaml.h>

#include "1000s.hpp"
#include "defs.hpp"
#include "mandelbrot.hpp"
#include "util.hpp"

namespace mp = boost::multiprecision;
namespace chrono = std::chrono;
using palette_t = std::vector<sf::Color>;

int num_threads = static_cast<int>(std::thread::hardware_concurrency());
double zoom_from = 0.25;
double zoom_to = 1000;
double zoom_factor = 1.0;
double zoom_increment = 0.12;
int file_index = 0;
double c_real = -0.75;
double c_imag = 0.0;
mp::mpfr_float c_real_mp{-0.75};
mp::mpfr_float c_imag_mp{0.0};
mpfr_prec_t min_precision_bits = 64;
double log_scale_factor = 0.1;
palette_t palette;
std::string data_file{};
std::string image_file{};
std::string checkpoint_file{};
YAML::Node config;

static constexpr double ZOOM_THRESHOLD_FOR_DOUBLE_PREC = 44.5;
static constexpr std::string APP_NAME{"AppleCore"};

void setup(void)
{
    if (config["checkpoint"]["file_index"])
    {
        file_index = config["checkpoint"]["file_index"].as<int>();
    }
    if (config["zoom"]["from"])
    {
        zoom_from = config["zoom"]["from"].as<double>();
    }
    if (config["zoom"]["to"])
    {
        zoom_to = config["zoom"]["to"].as<double>();
    }
    if (config["zoom"]["factor"])
    {
        zoom_factor = config["zoom"]["factor"].as<double>();
    }
    if (config["zoom"]["increment"])
    {
        zoom_increment = config["zoom"]["increment"].as<double>();
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
    if (config["data_file"])
    {
        data_file = config["data_file"].as<std::string>();
    }
    if (config["image_file"])
    {
        image_file = config["image_file"].as<std::string>();
    }
    if (config["checkpoint_file"])
    {
        checkpoint_file = config["checkpoint_file"].as<std::string>();
    }
    if (config["center"]["r"] && config["center"]["i"])
    {
        c_real = config["center"]["r"].as<double>();
        c_imag = config["center"]["i"].as<double>();
        c_real_mp.assign(config["center"]["r"].as<std::string>());
        c_imag_mp.assign(config["center"]["i"].as<std::string>());
    }
}

void setup(mandelbrot_computer_base& mandelbrot)
{
    if (config["width"] && config["height"])
    {
        mandelbrot.width = config["width"].as<int>();
        mandelbrot.height = config["height"].as<int>();
    }
    if (config["iterations"]["base"])
    {
        mandelbrot.base_iterations = config["iterations"]["base"].as<iteration_count_t>();
    }
    if (config["iterations"]["forced"])
    {
        mandelbrot.forced_max_iterations = config["iterations"]["forced"].as<iteration_count_t>();
    }
    if (config["iterations"]["limit"])
    {
        mandelbrot.max_iterations_limit = config["iterations"]["limit"].as<iteration_count_t>();
    }
    if (config["iterations"]["factor"])
    {
        mandelbrot.max_iter_factor = config["iterations"]["factor"].as<double>();
    }
    if (config["iterations"]["exponent"])
    {
        mandelbrot.max_iter_exponent = config["iterations"]["exponent"].as<double>();
    }
}

std::string process_filename_template(std::string const& filename, mandelbrot_computer_base const& mandelbrot,
                                      int file_index, iteration_count_t max_iterations, double zoom_level)
{
    std::string fidx = std::to_string(file_index);
    fidx = std::string(6U - fidx.length(), '0') + fidx;
    std::string result = replace_substring(filename, "{file_index}", fidx);
    result = replace_substring(result, "{max_iterations}", std::to_string(max_iterations));
    result = replace_substring(result, "{log_scale_factor}", std::to_string(log_scale_factor));
    result = replace_substring(result, "{zoom_level}", std::to_string(zoom_level));
    result =
        replace_substring(result, "{size}", std::to_string(mandelbrot.width) + 'x' + std::to_string(mandelbrot.height));
    return result;
}

int main(int argc, char* argv[])
{
    mandelbrot_calculator<double> mandelbrot_dp;
    mandelbrot_calculator<mp::mpfr_float> mandelbrot_mp;
    mandelbrot_computer_base* mandelbrot = &mandelbrot_mp;
    if (argc > 1)
    {
        config = YAML::LoadFile(argv[1]);
        setup();
        setup(mandelbrot_dp);
        setup(mandelbrot_mp);
    }
    mpfr_set_default_prec(min_precision_bits);
    double zoom_level = zoom_from;

    auto t0 = chrono::system_clock::now();
    std::cout << "Generating " << mandelbrot->width << 'x' << mandelbrot->height << " image in " << num_threads
              << " threads. ";
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    std::cout << "Zooming from " << zoom_from << " to " << zoom_to << '.' << std::endl;

    // Queue that holds the work items
    std::queue<work_item> work_queue;

    // Mutex to protect the queue
    std::mutex mtx;

    // Condition variable to signal when the queue is not empty
    std::condition_variable cv;

    // Create buffer for partial results
    std::vector<iteration_count_t> result_buffer(mandelbrot->width * mandelbrot->height);
    std::fill(std::begin(result_buffer), std::end(result_buffer), 0);

    // Launch worker threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&work_queue, &mtx, &cv, &mandelbrot, &zoom_level]() {
            while (true)
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&work_queue] { return !work_queue.empty(); });
                work_item item = work_queue.front();
                work_queue.pop();
                lock.unlock();
                if (item.quit)
                    break;
                mandelbrot->calculate_mandelbrot_row(item);
            }
        });
    }

    // Loop for zooming in
#ifndef HEADLESS
    sf::RenderWindow window(sf::VideoMode(mandelbrot->width / 2, mandelbrot->height / 2), APP_NAME);
    sf::Event event;
    window.clear(sf::Color::Green);
    window.display();
    (void)window.pollEvent(event);
    bool quit_on_next_frame = false;
    while (zoom_level <= zoom_to && window.isOpen() && !quit_on_next_frame)
#else
    double zoom_level = zoom_from;
    while (zoom_level <= zoom_to)
#endif
    {
        mandelbrot = (zoom_level < ZOOM_THRESHOLD_FOR_DOUBLE_PREC)
                         ? reinterpret_cast<mandelbrot_computer_base*>(&mandelbrot_dp)
                         : reinterpret_cast<mandelbrot_computer_base*>(&mandelbrot_mp);
        double scale_factor = 4.0 / std::pow(2.0, zoom_level) / std::max(mandelbrot->width, mandelbrot->height);
        double real_start = c_real - mandelbrot->width / 2.0 * scale_factor;
        double imag_start = c_imag - mandelbrot->height / 2.0 * scale_factor;
        mp::mpfr_float real_start_mp = c_real_mp - mandelbrot->width / 2.0 * scale_factor;
        mp::mpfr_float imag_start_mp = c_imag_mp - mandelbrot->height / 2.0 * scale_factor;
        mandelbrot->reset();
        const iteration_count_t max_iterations = mandelbrot->forced_max_iterations.value_or(
            std::min(mandelbrot->max_iterations_limit, mandelbrot->calculate_max_iterations(zoom_level)));

        std::cout << "\rZoom: " << std::setprecision(6) << std::defaultfloat << zoom_level
                  << "; Δpixel: " << std::scientific << std::setprecision(24) << scale_factor
                  << "; max. iterations: " << max_iterations << "; current file index: " << file_index << "\x1b[K"
                  << std::endl;
        auto frame_t0 = chrono::system_clock::now();

        // Add work items to queue
        for (int row = 0; row < mandelbrot->height; ++row)
        {
            std::lock_guard<std::mutex> lock(mtx);
            work_queue.emplace(work_item{.result = result_buffer.data() + row * mandelbrot->width,
                                         .scale_factor = scale_factor,
                                         .real_start = real_start,
                                         .imag_start = imag_start,
                                         .real_start_mp = real_start_mp,
                                         .imag_start_mp = imag_start_mp,
                                         .row = row,
                                         .max_iterations = max_iterations});
            cv.notify_one();
        }

#ifndef HEADLESS
        window.setTitle(APP_NAME + " [" + std::to_string(file_index) + "]");
        sf::Vector2i last_mouse_pos = sf::Mouse::getPosition(window);
        while (mandelbrot->completed_rows < mandelbrot->height && window.isOpen())
        {
            int last_completed_rows = mandelbrot->completed_rows;
            while (mandelbrot->completed_rows <= last_completed_rows && window.isOpen() &&
                   last_mouse_pos == sf::Mouse::getPosition(window))
            {
                sf::sleep(sf::milliseconds(100));
            }
            last_mouse_pos = sf::Mouse::getPosition(window);
            std::cout << "\r" << mandelbrot->completed_rows << " of " << mandelbrot->height << " rows completed ("
                      << std::fixed << std::setprecision(1) << (100.0 * mandelbrot->completed_rows / mandelbrot->height)
                      << "%)\x1b[K" << std::flush;
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
                        mp::mpfr_float const& pixel_real = real_start_mp + mouse_pos.x * scale_factor;
                        mp::mpfr_float const& pixel_imag = imag_start_mp + mouse_pos.y * scale_factor;
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
            sf::Image const& intermediate_image =
                colorize(result_buffer, mandelbrot->width, mandelbrot->height, mandelbrot->completed_rows,
                         max_iterations, get_rainbow_color);
            sf::Texture tex;
            tex.loadFromImage(intermediate_image);
            sf::Sprite sprite(tex);
            sprite.setScale(sf::Vector2f(0.5f, 0.5f));
            window.draw(sprite);
            window.display();
        }
#else
        while (mandelbrot.completed_rows < mandelbrot.height)
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
        }
#endif
        iteration_count_t total_iterations = std::reduce(result_buffer.cbegin(), result_buffer.cend());
        std::cout << "\rAverage iterations: " << (total_iterations / result_buffer.size()) << "\x1b[K\n";
        if (!image_file.empty())
        {
            sf::Image const& completed_image =
                colorize(result_buffer, mandelbrot->width, mandelbrot->height, mandelbrot->completed_rows - 1,
                         max_iterations, get_rainbow_color);
            std::string png_file =
                process_filename_template(image_file, *mandelbrot, file_index, max_iterations, zoom_level);
            std::cout << "Writing image to " << png_file << "\x1b[K" << std::flush;
            completed_image.saveToFile(png_file);
        }

        if (!data_file.empty())
        {
            std::string result_file =
                process_filename_template(data_file, *mandelbrot, file_index, max_iterations, zoom_level);
            std::cout << "Writing data to " << result_file << "\x1b[K" << std::flush;
            save_result(result_buffer, mandelbrot->width, mandelbrot->height, max_iterations, result_file);
        }

        auto now = chrono::system_clock::now();

        std::cout << "\rElapsed time: " << format_duration(now - frame_t0) << "\x1b[K" << std::endl;

        ++file_index;
        zoom_level = zoom_level * zoom_factor + zoom_increment;

        if (!checkpoint_file.empty())
        {
            config["zoom"]["from"] = zoom_level;
            config["checkpoint"]["file_index"] = file_index;
            config["checkpoint"]["zoom"] = 1.0 / scale_factor;
            config["checkpoint"]["t0"] = get_iso_timestamp(t0);
            config["checkpoint"]["now"] = get_current_iso_timestamp();
            config["checkpoint"]["elapsed_total"] = format_duration(now - t0);
            config["checkpoint"]["elapsed_last_frame"] = format_duration(now - frame_t0);
            std::string const& checkpoint_out_filename =
                process_filename_template(checkpoint_file, *mandelbrot, file_index, max_iterations, zoom_level);
            std::ofstream checkpoint(checkpoint_out_filename, std::ios::trunc);
            checkpoint << config;
        }
    }

    // Signal all threads to terminate
    for (int i = 0; i < num_threads; ++i)
    {
        std::lock_guard<std::mutex> lock(mtx);
        work_queue.emplace(work_item{.quit = true});
        cv.notify_one();
    }

    // Wait for threads to complete
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    return EXIT_SUCCESS;
}
