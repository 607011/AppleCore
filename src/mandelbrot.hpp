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

namespace mp = boost::multiprecision;

template <typename FloatType> struct mandelbrot_calculator : mandelbrot_computer_base
{
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

    void calculate_mandelbrot_row(work_item const& w) override
    {
        for (int x = 0; x < this->width; ++x)
        {
            FloatType const& pixel_real = w.real_start + w.scale_factor * x;
            FloatType const& pixel_imag = w.imag_start + w.scale_factor * w.row;
            w.result[x] = calculate(pixel_real, pixel_imag, w.max_iterations);
        }
        ++this->completed_rows;
    }
};

#endif // __MANDELBROT_HPP__
