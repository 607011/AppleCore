#ifndef __MANDELBROT_DEFS_HPP__
#define __MANDELBROT_DEFS_HPP__

#include <atomic>
#include <cstddef>
#include <iostream>
#include <optional>

#include <boost/multiprecision/mpfr.hpp>

typedef uint64_t iteration_count_t;

namespace
{
namespace mp = boost::multiprecision;

struct work_item
{
    iteration_count_t* result{nullptr};
    const double scale_factor{};
    const double real_start{};
    const double imag_start{};
    const mp::mpfr_float& real_start_mp{};
    const mp::mpfr_float& imag_start_mp{};
    const int row{};
    const int radius{1}; // needed by perturbative calculator
    const iteration_count_t max_iterations{};
    bool quit{false};
};

struct mandelbrot_computer_base
{
    iteration_count_t base_iterations{42};
    double max_iter_factor{17.40139};
    double max_iter_exponent{2.2};
    iteration_count_t max_iterations_limit{2'000'000'000ULL};
    std::optional<iteration_count_t> forced_max_iterations{};
    std::atomic<int> completed_rows{0};
    int width{3840};
    int height{2160};

    virtual void calculate_mandelbrot_row(work_item const&) = 0;

    void reset(void)
    {
        completed_rows = 0;
    }

    virtual ~mandelbrot_computer_base()
    {
    }

    iteration_count_t calculate_max_iterations(double zoom_level)
    {
        iteration_count_t max_iterations =
            100 + static_cast<iteration_count_t>(max_iter_factor * std::pow(zoom_level, max_iter_exponent));
        return max_iterations;
    }
};

} // namespace

#endif // __MANDELBROT_DEFS_HPP__
