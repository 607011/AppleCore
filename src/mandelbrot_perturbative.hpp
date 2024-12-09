#ifndef __MANDELBROT_PERTURBATIVE_HPP__
#define __MANDELBROT_PERTURBATIVE_HPP__

template <typename FloatType> struct mandelbrot_calculator_perturbative
{
    using ComplexType = std::complex<FloatType>;

    struct thread_param
    {
        sf::Image& image;
        const double scale_factor;
        FloatType const& real_start;
        FloatType const& imag_start;
        const int start_row;
        const int end_row;
        const int radius{1}; // needed by perturbative calculator
        const iteration_count_t max_iterations;
    };
    iteration_count_t base_iterations{1000};
    double log_scale_factor{0.1};
    iteration_count_t max_iterations_limit{2'000'000'000ULL};
    std::atomic<int> completed_rows{0};
    int width{3840};
    int height{2160};

    struct ExplorationResult
    {
        int x;
        int y;
        iteration_count_t iterations;
    };

    iteration_count_t calculate_max_iterations(double zoom_level)
    {
        iteration_count_t max_iterations =
            static_cast<iteration_count_t>(base_iterations * std::exp(log_scale_factor * zoom_level));
        return max_iterations;
    }

    struct ReferenceOrbit
    {
        std::vector<ComplexType> trajectory;
        iteration_count_t reference_iterations;

        void compute(ComplexType const& center, iteration_count_t max_iterations)
        {
            trajectory.clear();
            FloatType x = 0;
            FloatType y = 0;
            FloatType x2 = 0;
            FloatType y2 = 0;
            iteration_count_t iterations = 0;
            while (x2 + y2 <= 4 && iterations < max_iterations)
            {
                trajectory.emplace_back(x, y);
                y = 2 * x * y + center.imag();
                x = x2 - y2 + center.real();
                x2 = x * x;
                y2 = y * y;
                ++iterations;
            }
            reference_iterations = iterations;
        }
    };

    iteration_count_t approximate_iterations(const ReferenceOrbit& reference, const ComplexType& delta_c)
    {
        using PerturbationType = std::complex<double>;
        PerturbationType z_perturb(0, 0);
        PerturbationType delta(static_cast<double>(delta_c.real()), static_cast<double>(delta_c.imag()));
        for (size_t n = 0; n < reference.trajectory.size(); ++n)
        {
            PerturbationType ref_z(static_cast<double>(reference.trajectory.at(n).real()),
                                   static_cast<double>(reference.trajectory.at(n).imag()));
            z_perturb = 2.0 * ref_z * z_perturb + z_perturb * z_perturb + delta;
            if (std::norm(z_perturb) > 4)
                return n;
        }
        return reference.reference_iterations;
    }

    std::vector<ExplorationResult> explore_neighborhood(ComplexType const& center, FloatType scale_factor,
                                                        const int radius, const iteration_count_t max_iterations)
    {
        ReferenceOrbit reference;
        reference.compute(center, max_iterations);
        std::vector<ExplorationResult> results;
        for (int i = -radius; i <= radius; ++i)
        {
            for (int j = -radius; j <= radius; ++j)
            {
                ComplexType delta_c(scale_factor * i, scale_factor * j);
                iteration_count_t approx_iterations = approximate_iterations(reference, delta_c);
                ComplexType point = center + delta_c;
                results.emplace_back(i, j, approx_iterations);
            }
        }
        return results;
    }

    void calculate_mandelbrot_row_range(thread_param const& p)
    {
        const int r = p.radius;
        FloatType const& scale_factor = p.scale_factor;
        for (int y = p.start_row + r; y <= p.end_row - r; y += 2 * r)
        {
            for (int x = 0; x < width - r; x += 2 * r)
            {
                ComplexType center(p.real_start + scale_factor * x, p.imag_start + scale_factor * y);
                std::vector<ExplorationResult> const& exploration_results =
                    explore_neighborhood(center, scale_factor, r, p.max_iterations);
                for (auto [dx, dy, iterations] : exploration_results)
                {
                    const int px = x + dx;
                    const int py = y + dy;
                    if (px < 0 || px >= width || py < p.start_row || py >= p.end_row)
                        continue;
                    const double hue = static_cast<double>(iterations) / static_cast<double>(p.max_iterations);
                    p.image.setPixel(px, py - p.start_row,
                                     (iterations < p.max_iterations) ? get_rainbow_color(hue) : sf::Color::Black);
                }
            }
            completed_rows += 2 * r + 1;
        }
    }
};

#endif // __MANDELBROT_PERTURBATIVE_HPP__
