#ifndef __MANDELBROT_DEFS_HPP__
#define __MANDELBROT_DEFS_HPP__

#include <cstddef>

typedef uint64_t iteration_count_t;

template <typename FloatType> struct work_item
{
    iteration_count_t *result{nullptr};
    const double scale_factor{};
    FloatType const& real_start{};
    FloatType const& imag_start{};
    const int row{};
    const int radius{1}; // needed by perturbative calculator
    const iteration_count_t max_iterations{};
    bool quit{false};
};


#endif // __MANDELBROT_DEFS_HPP__
