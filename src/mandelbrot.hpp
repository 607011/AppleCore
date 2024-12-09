#ifndef __MANDELBROT_HPP__
#define __MANDELBROT_HPP__

#include <atomic>
#include <cmath>
#include <iostream>
#include <mutex>
#include <sys/types.h>
#include <utility>
#include <vector>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>

#include "util.hpp"

namespace
{

using iteration_count_t = uint64_t;
namespace mp = boost::multiprecision;

template <typename FloatType> struct work_item
{
    sf::Image& image;
    const double scale_factor{};
    FloatType const& real_start{};
    FloatType const& imag_start{};
    const int row{};
    const int radius{1}; // needed by perturbative calculator
    const iteration_count_t max_iterations{};
    bool quit{false};
};

template <typename FloatType> struct mandelbrot_calculator
{
    iteration_count_t base_iterations = 100;
    double log_scale_factor = 0.25;
    iteration_count_t max_iterations_limit = 2'000'000'000ULL;
    std::atomic<int> completed_rows = 0;
    int width = 3840;
    int height = 2160;

    void reset(void)
    {
        completed_rows = 0;
    }

    inline iteration_count_t calculate(FloatType const& x0, FloatType const& y0, const iteration_count_t max_iterations)
    {
        FloatType x = 0;
        FloatType y = 0;
        FloatType x2 = 0;
        FloatType y2 = 0;
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

    void calculate_mandelbrot_row(work_item<FloatType> const& w)
    {
        for (int x = 0; x < width; ++x)
        {
            FloatType const& pixel_real = w.real_start + w.scale_factor * x;
            FloatType const& pixel_imag = w.imag_start + w.scale_factor * w.row;
            const iteration_count_t iterations = calculate(pixel_real, pixel_imag, w.max_iterations);
            const double hue = static_cast<double>(iterations) / static_cast<double>(w.max_iterations);
            w.image.setPixel(x, 0, (iterations < w.max_iterations) ? get_rainbow_color(hue) : sf::Color::Black);
        }
        ++completed_rows;
    }

    iteration_count_t calculate_max_iterations(double zoom_level)
    {
        iteration_count_t max_iterations =
            static_cast<iteration_count_t>(base_iterations * std::exp(log_scale_factor * zoom_level));
        return max_iterations;
    }
};

} // namespace

#endif // __MANDELBROT_HPP__
