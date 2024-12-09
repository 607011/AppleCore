#ifndef __MANDELBROT_RENDERER__METAL_H__
#define __MANDELBROT_RENDERER__METAL_H__

#include <simd/simd.h>

using iteration_count_t = uint64_t;

template <typename FloatType> struct work_item
{
    sf::Image& image;
    const FloatType scale_factor{};
    FloatType const& real_start{};
    FloatType const& imag_start{};
    const int row{};
    const int radius{1}; // needed by perturbative calculator
    const iteration_count_t max_iterations{};
    bool quit{false};
};

struct mandelbrot_calculator_metal
{
    iteration_count_t base_iterations = 100;
    double log_scale_factor = 0.25;
    iteration_count_t max_iterations_limit = 2'000'000'000ULL;
    std::atomic<int> completed_rows = 0;
    int width = 3840;
    int height = 2160;

    mandelbrot_calculator_metal();

    void reset(void)
    {
        completed_rows = 0;
    }
    void render(int width, int height, simd_float2 center, float zoom, int max_iterations);

    void calculate_mandelbrot_row(work_item<double> const& w);
    iteration_count_t calculate_max_iterations(double zoom_level);
};

#endif // __MANDELBROT_RENDERER__METAL_H__