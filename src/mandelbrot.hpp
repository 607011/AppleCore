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

#include "defs.hpp"
#include "util.hpp"

namespace
{
namespace mp = boost::multiprecision;

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
            w.result[x] = calculate(pixel_real, pixel_imag, w.max_iterations);
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
