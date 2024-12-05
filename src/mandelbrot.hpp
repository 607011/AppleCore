#ifndef __MANDELBROT_HPP__
#define __MANDELBROT_HPP__

#include <atomic>
#include <cmath>
#include <complex>
#include <sys/types.h>
#include <utility>
#include <vector>

#include <boost/multiprecision/mpfr.hpp>

#include "util.hpp"

namespace
{

using iteration_count_t = uint64_t;
namespace mp = boost::multiprecision;

struct thread_param
{
    sf::Image& image;
    double scale_factor; 
    mp::mpfr_float const& real_start;
    mp::mpfr_float const& imag_start;
    int start_row;
    int end_row;
    const iteration_count_t max_iterations;
};

struct mandelbrot_calculator
{
    iteration_count_t base_iterations = 1000;
    double log_scale_factor = 0.1;
    iteration_count_t max_iterations_limit = 2'000'000'000ULL;
    std::atomic<int> completed_rows = 0;
    int width = 3840;
    int height = 2160;

    iteration_count_t calculate(mp::mpfr_float const& x0, mp::mpfr_float const& y0,
                                const iteration_count_t max_iterations)
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

    void calculate_mandelbrot_row_range(thread_param const& p)
    {
        for (int y = p.start_row; y < p.end_row; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                mp::mpfr_float const& pixel_real = p.real_start + p.scale_factor * x;
                mp::mpfr_float const& pixel_imag = p.imag_start + p.scale_factor * y;
                iteration_count_t iterations = calculate(pixel_real, pixel_imag, p.max_iterations);
                const double hue = static_cast<double>(iterations) / static_cast<double>(p.max_iterations);
                p.image.setPixel(x, y - p.start_row,
                                 (iterations < p.max_iterations) ? get_rainbow_color(hue) : sf::Color::Black);
            }
            ++completed_rows;
        }
    }

    iteration_count_t calculate_max_iterations(double zoom_level)
    {
        iteration_count_t max_iterations =
            static_cast<iteration_count_t>(base_iterations * std::exp(log_scale_factor * zoom_level));
        return max_iterations;
    }
};

template <typename FloatType> class MandelbrotNeighborhood
{
  public:
    using ComplexType = std::complex<FloatType>;
    // Compute reference orbit for base point
    struct ReferenceOrbit
    {
        std::vector<FloatType> x_trajectory;
        std::vector<FloatType> y_trajectory;
        iteration_count_t reference_iterations;

        void compute(const FloatType& x0, const FloatType& y0, iteration_count_t max_iterations)
        {
            x_trajectory.clear();
            y_trajectory.clear();

            FloatType x = 0;
            FloatType y = 0;
            FloatType x2 = 0;
            FloatType y2 = 0;
            iteration_count_t iterations = 0;

            while (x2 + y2 <= 4 && iterations < max_iterations)
            {
                // Store trajectory points
                x_trajectory.push_back(x);
                y_trajectory.push_back(y);

                y = 2 * x * y + y0;
                x = x2 - y2 + x0;
                x2 = x * x;
                y2 = y * y;
                ++iterations;
            }

            reference_iterations = iterations;
        }
    };

    // Perturbation computation for nearby points
    iteration_count_t approximate_iterations(const ReferenceOrbit& reference, const ComplexType& delta_c,
                                             iteration_count_t max_iterations)
    {
        // Small perturbation types (could be double or float)
        using PerturbationType = std::complex<double>;

        PerturbationType z_perturb(0, 0);
        PerturbationType delta(static_cast<double>(delta_c.real()), static_cast<double>(delta_c.imag()));

        for (size_t n = 0; n < reference.trajectory.size(); ++n)
        {
            // Convert reference trajectory to lower precision
            PerturbationType ref_z(static_cast<double>(reference.trajectory[n].real()),
                                   static_cast<double>(reference.trajectory[n].imag()));

            // Compute perturbed iteration using linearized approximation
            z_perturb = 2.0 * ref_z * z_perturb + z_perturb * z_perturb + delta;

            // Escape condition for perturbed point
            if (std::norm(z_perturb) > 4)
            {
                return n;
            }
        }

        // If we didn't escape, return max iterations
        return reference.reference_iterations;
    }

    // Neighborhood exploration method
    std::vector<std::pair<ComplexType, iteration_count_t>> explore_neighborhood(const ComplexType& center,
                                                                                const FloatType& radius,
                                                                                iteration_count_t max_iterations,
                                                                                int resolution = 10)
    {
        // Compute reference orbit
        ReferenceOrbit reference;
        reference.compute(center, max_iterations);

        std::vector<std::pair<ComplexType, iteration_count_t>> results;

        // Generate grid of points around the center
        for (int i = 0; i < resolution; ++i)
        {
            for (int j = 0; j < resolution; ++j)
            {
                // Interpolate points in the neighborhood
                FloatType dx = radius * (2.0 * i / (resolution - 1) - 1.0);
                FloatType dy = radius * (2.0 * j / (resolution - 1) - 1.0);

                ComplexType delta_c(dx, dy);
                ComplexType point = center + delta_c;

                // Approximate iterations using perturbation
                iteration_count_t approx_iterations = approximate_iterations(reference, delta_c, max_iterations);

                results.push_back({point, approx_iterations});
            }
        }

        return results;
    }
};

} // namespace

#endif // __MANDELBROT_HPP__
