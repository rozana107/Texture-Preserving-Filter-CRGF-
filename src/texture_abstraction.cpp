#include "texture_abstraction.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace {

struct QueueEntry {
    int pixel;
    double distance;

    QueueEntry(int pixelIndex, double pathDistance)
        : pixel(pixelIndex), distance(pathDistance) {}
};

// std::priority_queue normally puts the largest item first. Reverse that so
// that the pixel with the smallest path cost is processed first.
struct SmallestDistanceFirst {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
        if (a.distance != b.distance) {
            return a.distance > b.distance;
        }
        return a.pixel > b.pixel; // Stable, row-major ordering for equal costs.
    }
};

double colorDistance(const unsigned char* a, const unsigned char* b,
                     int channels) {
    double squaredDistance = 0.0;
    for (int c = 0; c < channels; ++c) {
        const double difference = double(a[c]) - double(b[c]);
        squaredDistance += difference * difference;
    }
    return std::sqrt(squaredDistance);
}

} // namespace

cv::Mat texturePreservingAbstraction(const cv::Mat& image,
                                    int maskSize, double gamma) {
    if (image.empty() || image.dims != 2 ||
        (image.type() != CV_8UC1 && image.type() != CV_8UC3)) {
        throw std::invalid_argument("Expected an 8-bit grayscale or BGR image.");
    }
    if (maskSize < 1) {
        throw std::invalid_argument("Mask size must be at least 1.");
    }
    if (!std::isfinite(gamma) || gamma < 0.0) {
        throw std::invalid_argument("Gamma must be finite and nonnegative.");
    }
    if (image.total() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("Image has too many pixels for integer indices.");
    }

    const int width = image.cols;
    const int height = image.rows;
    const int pixelCount = static_cast<int>(image.total());
    const int channels = image.channels();
    maskSize = std::min(maskSize, pixelCount);
    if (maskSize == 1) {
        return image.clone();
    }

    // A cropped cv::Mat can have gaps between rows. Make a continuous copy only
    // when necessary, so a pixel's bytes start at data + pixelIndex * channels.
    const cv::Mat source = image.isContinuous() ? image : image.clone();
    const unsigned char* data = source.ptr<unsigned char>(0);
    cv::Mat output(height, width, image.type());
    unsigned char* result = output.ptr<unsigned char>(0);

    std::vector<double> bestDistance(pixelCount, 0.0);
    std::vector<int> distanceStamp(pixelCount, -1);
    std::vector<int> acceptedStamp(pixelCount, -1);

    // Four-connected grid: left, right, up, down. Each move is one grid step.
    const int dx[4] = {-1, 1, 0, 0};
    const int dy[4] = {0, 0, -1, 1};

    for (int center = 0; center < pixelCount; ++center) {
        const unsigned char* centerColor = data + std::size_t(center) * channels;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                            SmallestDistanceFirst> frontier;

        // Stamps identify this center's search without clearing image-sized
        // arrays for every pixel. An entry is valid only if its stamp == center.
        bestDistance[center] = 0.0;
        distanceStamp[center] = center;
        frontier.push(QueueEntry(center, 0.0));

        double sum[3] = {0.0, 0.0, 0.0};
        int acceptedCount = 0;

        while (!frontier.empty() && acceptedCount < maskSize) {
            const QueueEntry current = frontier.top();
            frontier.pop();

            // A shorter route may have been queued since this entry was added.
            // Accept each pixel once, only when its shortest distance is known.
            if (acceptedStamp[current.pixel] == center ||
                current.distance > bestDistance[current.pixel]) {
                continue;
            }
            acceptedStamp[current.pixel] = center;

            const unsigned char* currentColor =
                data + std::size_t(current.pixel) * channels;
            for (int c = 0; c < channels; ++c) {
                sum[c] += currentColor[c];
            }
            ++acceptedCount;
            if (acceptedCount == maskSize) {
                break;
            }

            const int x = current.pixel % width;
            const int y = current.pixel / width;
            for (int direction = 0; direction < 4; ++direction) {
                const int nx = x + dx[direction];
                const int ny = y + dy[direction];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    continue;
                }
                const int neighbor = ny * width + nx;
                if (acceptedStamp[neighbor] == center) {
                    continue;
                }
                const unsigned char* neighborColor =
                    data + std::size_t(neighbor) * channels;

                // Equation (3): the first difference is always relative to the
                // ORIGINAL CENTER, not to the previous pixel on the path.
                const double stepCost = colorDistance(neighborColor, centerColor, channels)
                    + gamma * colorDistance(neighborColor, currentColor, channels);
                const double candidate = current.distance + stepCost;
                if (!std::isfinite(candidate)) {
                    throw std::overflow_error("Path cost overflow; use a smaller gamma.");
                }

                if (distanceStamp[neighbor] != center ||
                    candidate < bestDistance[neighbor]) {
                    distanceStamp[neighbor] = center;
                    bestDistance[neighbor] = candidate;
                    frontier.push(QueueEntry(neighbor, candidate));
                }
            }
        }

        // Equation (4): uniform average over exactly maskSize accepted pixels.
        // All searches read source; no filtered pixel feeds into another search.
        for (int c = 0; c < channels; ++c) {
            result[std::size_t(center) * channels + c] =
                cv::saturate_cast<unsigned char>(sum[c] / acceptedCount);
        }
    }

    return output;
}
