#include "texture_abstraction.h"

#include <opencv2/imgcodecs.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void printUsage(std::ostream& stream) {
    stream << "Texture-preserving abstraction (David Mould, NPAR 2012)\n\n"
           << "Usage: texture_abstraction input output [mask_size] [gamma]\n\n"
           << "  input       Input image (8-bit grayscale or BGR color).\n"
           << "  output      Output path, for example result.png.\n"
           << "  mask_size   Pixels per neighborhood, including its center.\n"
           << "              Default: 160. Must be a positive integer.\n"
           << "  gamma       Local edge-crossing weight. Default: 1.0.\n"
           << "              Must be finite and nonnegative.\n\n"
           << "Example: texture_abstraction photo.jpg result.png 160 1.0\n";
}

int parseMaskSize(const char* text) {
    char* end = NULL;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 1 ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("Mask size must be a positive integer.");
    }
    return static_cast<int>(value);
}

double parseGamma(const char* text) {
    char* end = NULL;
    errno = 0;
    const double value = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' ||
        !std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument("Gamma must be a finite, nonnegative number.");
    }
    return value;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--help" ||
                      std::string(argv[1]) == "-h")) {
        printUsage(std::cout);
        return EXIT_SUCCESS;
    }
    if (argc < 3 || argc > 5) {
        printUsage(std::cerr);
        return EXIT_FAILURE;
    }

    try {
        const int maskSize = argc >= 4 ? parseMaskSize(argv[3]) : 160;
        const double gamma = argc >= 5 ? parseGamma(argv[4]) : 1.0;
        // Preserve the original depth and channels so unsupported inputs can be
        // reported explicitly rather than silently discarding alpha or precision.
        const cv::Mat input = cv::imread(argv[1], cv::IMREAD_UNCHANGED);
        if (input.empty()) {
            throw std::runtime_error("Could not read input image: " + std::string(argv[1]));
        }

        std::cout << "Filtering " << input.cols << " x " << input.rows
                  << " image, mask size " << maskSize << ", gamma " << gamma
                  << "..." << std::endl;
        const double start = double(cv::getTickCount());
        const cv::Mat output = texturePreservingAbstraction(input, maskSize, gamma);
        const double seconds = (double(cv::getTickCount()) - start) / cv::getTickFrequency();

        if (!cv::imwrite(argv[2], output)) {
            throw std::runtime_error("Could not write output image: " + std::string(argv[2]));
        }
        std::cout << "Saved " << argv[2] << " (filter time: " << seconds << " seconds).\n";
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
