#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <thread>
#include <vector>

#include <gmpxx.h>
#include <opencv2/opencv.hpp>
#include <yaml-cpp/yaml.h>

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

long long mandelbrot(mpf_class const& c_real, mpf_class const& c_imag, const long long max_iterations)
{
    mpf_class z_real = 0;
    mpf_class z_imag = 0;
    long long iterations = 0;
    while (iterations < max_iterations)
    {
        // calculate z^2
        mpf_class z_real_temp = z_real * z_real - z_imag * z_imag;
        mpf_class z_imag_temp = 2 * z_real * z_imag;
        // add c
        z_real = z_real_temp + c_real;
        z_imag = z_imag_temp + c_imag;
        // check for divergence
        if (z_real * z_real + z_imag * z_imag > 4)
            break;
        ++iterations;
    }
    return iterations;
}

unsigned long long calculate_max_iterations(double zoom_level)
{
    const long long base_iterations = 1000;
    const double scale_factor = 1.5;
    unsigned long long max_iterations =
        base_iterations * static_cast<unsigned long long>(std::pow(scale_factor, zoom_level));
    return max_iterations;
}

std::vector<cv::Vec3b> create_grayscale_palette(void)
{
    std::vector<cv::Vec3b> palette;
    for (int i = 0; i < 256; ++i)
        palette.emplace_back(cv::Vec3b((uchar)i, (uchar)i, (uchar)i));
    return palette;
}

std::string replace_substring(const std::string& str, const std::string& substring, int value, int padding)
{
    std::string result = str;
    std::size_t pos = result.find(substring);
    while (pos != std::string::npos)
    {
        std::stringstream ss;
        ss << std::setw(padding) << std::setfill('0') << value;
        result.replace(pos, substring.length(), ss.str());
        pos = result.find(substring, pos + padding);
    }
    return result;
}

int main(int argc, char* argv[])
{
    int width = 4096;
    int height = 2160;
    double zoom_from = 0.5;
    double zoom_to = 1.84e100;
    double zoom_factor = 1.1;
    double zoom_increment = 0;
    mpf_class c_real("-0.75");
    mpf_class c_imag("0.0");
    std::string config_file = "config.yaml";
    mp_bitcnt_t min_precision_bits = 256;
    unsigned long long max_iterations_limit = 1'500'000'000ULL;
    std::vector<cv::Vec3b> palette = create_grayscale_palette();
    std::string out_file = "mandelbrot.png";
    if (argc > 1)
    {
        config_file = argv[1];
    }
    YAML::Node config = YAML::LoadFile(config_file);
    if (config["width"] && config["height"])
    {
        width = config["width"].as<int>();
        height = config["height"].as<int>();
    }
    if (config["max_iterations_limit"])
    {
        max_iterations_limit = config["max_iterations_limit"].as<unsigned long long>();
    }
    if (config["zoom"] && config["zoom"]["from"] && config["zoom"]["to"] && config["zoom"]["factor"])
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
    if (config["palette"] || config["palette"].IsSequence())
    {
        std::function<std::vector<uchar>(std::string)> parse_rgb = [&](std::string const& str) {
            std::vector<uchar> numbers;
            std::stringstream ss(str);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                int number;
                std::stringstream token_stream(token);
                token_stream >> number;
                numbers.push_back(static_cast<uchar>(number));
            }
            return numbers;
        };
        palette.clear();
        for (auto it : config["palette"])
        {
            std::vector<uchar> rgb = parse_rgb(it.as<std::string>());
            if (rgb.size() == 3)
            {
                palette.emplace_back(cv::Vec3b(rgb.at(0), rgb.at(1), rgb.at(2)));
            }
        }
    }
    if (config["out_file"])
    {
        out_file = config["out_file"].as<std::string>();
    }
    std::cout << "Generating " << width << 'x' << height << " image. ";
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    std::cout << "Zooming from " << zoom_from << " to " << zoom_to << '.' << std::endl;
    int zoom = 0;
    mpf_set_default_prec(min_precision_bits);
    static const cv::Vec3b BLACK = cv::Vec3b(0, 0, 0);
    for (double zoom_level = zoom_from; zoom_level < zoom_to; zoom_level = zoom_level * zoom_factor + zoom_increment)
    {
        std::cout << "Current zoom level: " << zoom_level << std::endl;
        cv::Mat image(height, width, CV_8UC3);
        mpf_class scale_factor = 4.0 / std::pow(2.0, zoom_level) / std::max(width, height);
        mpf_class aspect_ratio(static_cast<double>(width) / height);
        mpf_class real_start = c_real - width / 2.0 * scale_factor;
        mpf_class imag_start = c_imag - height / 2.0 * scale_factor;
        const unsigned long long max_iterations = std::min(max_iterations_limit, calculate_max_iterations(zoom_level));
        for (int y = 0; y < height; ++y)
        {
            std::cout << "\rProcessing row " << (y + 1) << " ... " << std::flush;
            for (int x = 0; x < width; ++x)
            {
                mpf_class const& pixel_real = real_start + x * scale_factor;
                mpf_class const& pixel_imag = imag_start + y * scale_factor;
                const unsigned long long iterations = mandelbrot(pixel_real, pixel_imag, max_iterations);
                cv::Vec3b color = (iterations < max_iterations)
                                      ? palette.at(static_cast<int>(iterations * palette.size() / max_iterations))
                                      : BLACK;
                image.at<cv::Vec3b>(y, x) = color;
            }
        }
        std::string png_file = replace_substring(out_file, "%z", zoom, 6);
        ++zoom;
        std::cout << "\rWriting image to " << png_file << "\x1b[K" << std::endl;
        cv::imwrite(png_file, image);
        cv::namedWindow("Display Image", cv::WINDOW_NORMAL);
        cv::imshow("Display Image", image);
    }

    cv::waitKey(0);
    cv::destroyAllWindows();

    return EXIT_SUCCESS;
}
