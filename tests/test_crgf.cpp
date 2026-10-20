#include "texture_abstraction.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void checkEqual(const cv::Mat& actual, const cv::Mat& expected,
                const std::string& message) {
    check(actual.size() == expected.size() && actual.type() == expected.type(),
          message + " (size or type)");
    check(cv::norm(actual, expected, cv::NORM_INF) == 0.0, message);
}

// Independent reference for tiny images: Bellman-Ford relaxes ALL grid edges
// repeatedly, then sorts the final distances. It does not use a priority queue,
// early stopping, or the production implementation's search stamps.
cv::Mat referenceFilter(const cv::Mat& input, int maskSize, double gamma) {
    const int count = input.rows * input.cols;
    const int channels = input.channels();
    maskSize = std::min(maskSize, count);
    cv::Mat output(input.size(), input.type());

    for (int center = 0; center < count; ++center) {
        const unsigned char* origin = input.ptr<unsigned char>(center / input.cols)
            + (center % input.cols) * channels;
        std::vector<double> distance(count, std::numeric_limits<double>::infinity());
        distance[center] = 0.0;

        for (int pass = 0; pass < count - 1; ++pass) {
            bool changed = false;
            for (int from = 0; from < count; ++from) {
                for (int to = 0; to < count; ++to) {
                    const int dx = std::abs(from % input.cols - to % input.cols);
                    const int dy = std::abs(from / input.cols - to / input.cols);
                    if (dx + dy != 1) {
                        continue;
                    }
                    const unsigned char* a = input.ptr<unsigned char>(from / input.cols)
                        + (from % input.cols) * channels;
                    const unsigned char* b = input.ptr<unsigned char>(to / input.cols)
                        + (to % input.cols) * channels;
                    double originDifference = 0.0;
                    double edgeDifference = 0.0;
                    for (int c = 0; c < channels; ++c) {
                        originDifference += (double(b[c]) - origin[c]) * (double(b[c]) - origin[c]);
                        edgeDifference += (double(b[c]) - a[c]) * (double(b[c]) - a[c]);
                    }
                    const double candidate = distance[from] + std::sqrt(originDifference)
                        + gamma * std::sqrt(edgeDifference);
                    if (candidate < distance[to]) {
                        distance[to] = candidate;
                        changed = true;
                    }
                }
            }
            if (!changed) {
                break;
            }
        }

        std::vector<std::pair<double, int> > sorted;
        for (int pixel = 0; pixel < count; ++pixel) {
            sorted.push_back(std::make_pair(distance[pixel], pixel));
        }
        std::sort(sorted.begin(), sorted.end());
        for (int c = 0; c < channels; ++c) {
            double sum = 0.0;
            for (int k = 0; k < maskSize; ++k) {
                const int pixel = sorted[k].second;
                sum += input.ptr<unsigned char>(pixel / input.cols)
                    [(pixel % input.cols) * channels + c];
            }
            output.ptr<unsigned char>(center / input.cols)
                [(center % input.cols) * channels + c] =
                    cv::saturate_cast<unsigned char>(sum / maskSize);
        }
    }
    return output;
}

void checkRejected(const cv::Mat& image, int maskSize, double gamma) {
    bool rejected = false;
    try {
        texturePreservingAbstraction(image, maskSize, gamma);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "Invalid input was accepted");
}

void testReferenceAgreement() {
    cv::RNG random(12345);
    const int masks[] = {1, 2, 5, 12, 30};
    const double gammas[] = {0.0, 0.5, 1.0, 8.0};
    int comparisons = 0;
    for (int channels = 1; channels <= 3; channels += 2) {
        for (int sample = 0; sample < 6; ++sample) {
            cv::Mat image(3, 4, CV_MAKETYPE(CV_8U, channels));
            random.fill(image, cv::RNG::UNIFORM, 0, 256);
            const cv::Mat original = image.clone();
            for (int m = 0; m < 5; ++m) {
                for (int g = 0; g < 4; ++g) {
                    checkEqual(texturePreservingAbstraction(image, masks[m], gammas[g]),
                               referenceFilter(image, masks[m], gammas[g]),
                               "Does not match Bellman-Ford reference");
                    ++comparisons;
                }
            }
            checkEqual(image, original, "Input was modified");
        }
    }
    std::cout << "Passed " << comparisons << " independent reference comparisons.\n";
}

void testEdgesAndFading() {
    // Both sides of a weak edge have enough pixels to fill their own masks.
    cv::Mat weakEdge(6, 8, CV_8UC1, cv::Scalar(80));
    weakEdge.colRange(4, 8).setTo(cv::Scalar(84));
    checkEqual(texturePreservingAbstraction(weakEdge, 12), weakEdge,
               "Weak edge should remain sharp");

    // A long, one-pixel-wide feature can fill its mask without crossing an edge.
    cv::Mat line(9, 9, CV_8UC3, cv::Scalar(200, 180, 160));
    line.col(4).setTo(cv::Scalar(20, 30, 40));
    checkEqual(texturePreservingAbstraction(line, 7), line,
               "Thin extended feature should survive");

    // A small dark feature fades, while its light neighbors remain unchanged.
    unsigned char values[] = {200, 200, 0, 200, 200};
    unsigned char expected[] = {200, 200, 100, 200, 200};
    checkEqual(texturePreservingAbstraction(cv::Mat(1, 5, CV_8UC1, values), 2),
               cv::Mat(1, 5, CV_8UC1, expected), "Asymmetric fading failed");

    // Similar colors separated by a barrier cannot be collected by jumping it.
    unsigned char barrier[] = {0, 200, 0};
    checkEqual(texturePreservingAbstraction(cv::Mat(1, 3, CV_8UC1, barrier), 2),
               cv::Mat(1, 3, CV_8UC1, cv::Scalar(100)), "Path barrier was ignored");
}

void testSizesAndMemoryLayout() {
    cv::Mat constant(4, 5, CV_8UC3, cv::Scalar(17, 83, 219));
    checkEqual(texturePreservingAbstraction(constant, 500), constant,
               "Constant image changed");
    cv::Mat single(1, 1, CV_8UC1, cv::Scalar(42));
    checkEqual(texturePreservingAbstraction(single), single, "Single pixel changed");

    cv::Mat parent(7, 8, CV_8UC3);
    cv::RNG random(99);
    random.fill(parent, cv::RNG::UNIFORM, 0, 256);
    const cv::Mat roi = parent(cv::Rect(2, 1, 4, 5));
    check(!roi.isContinuous(), "Test ROI must have row padding");
    checkEqual(texturePreservingAbstraction(roi, 6),
               texturePreservingAbstraction(roi.clone(), 6), "ROI row stride was ignored");

    cv::Mat identity = texturePreservingAbstraction(parent, 1);
    checkEqual(identity, parent, "Mask size 1 must be an identity");
    identity.setTo(cv::Scalar(0, 0, 0));
    check(cv::norm(identity, parent, cv::NORM_INF) > 0.0,
          "Output must own its memory");

    unsigned char rowValues[] = {0, 40, 100, 200};
    const cv::Mat row(1, 4, CV_8UC1, rowValues);
    checkEqual(texturePreservingAbstraction(row, 100),
               cv::Mat(1, 4, CV_8UC1, cv::Scalar(85)), "Oversized mask mean failed");
    cv::Mat column = row.t();
    checkEqual(texturePreservingAbstraction(column, 3), referenceFilter(column, 3, 1.0),
               "Single-column boundaries failed");
}

void testInvalidInputs() {
    const cv::Mat good(2, 2, CV_8UC1, cv::Scalar(0));
    checkRejected(cv::Mat(), 2, 1.0);
    checkRejected(cv::Mat(2, 2, CV_32FC1), 2, 1.0);
    checkRejected(cv::Mat(2, 2, CV_8UC4), 2, 1.0);
    const int sizes[] = {2, 2, 2};
    checkRejected(cv::Mat(3, sizes, CV_8UC1), 2, 1.0);
    checkRejected(good, 0, 1.0);
    checkRejected(good, -5, 1.0);
    checkRejected(good, 2, -1.0);
    checkRejected(good, 2, std::numeric_limits<double>::quiet_NaN());
    checkRejected(good, 2, std::numeric_limits<double>::infinity());

    unsigned char values[] = {0, 255};
    bool overflowRejected = false;
    try {
        texturePreservingAbstraction(cv::Mat(1, 2, CV_8UC1, values), 2,
                                    std::numeric_limits<double>::max());
    } catch (const std::overflow_error&) {
        overflowRejected = true;
    }
    check(overflowRejected, "Overflowing path cost was accepted");
}

} // namespace

int main() {
    try {
        testReferenceAgreement();
        testEdgesAndFading();
        testSizesAndMemoryLayout();
        testInvalidInputs();
        std::cout << "All CRGF checks passed.\n";
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
