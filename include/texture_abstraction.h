#ifndef TEXTURE_ABSTRACTION_H
#define TEXTURE_ABSTRACTION_H

#include <opencv2/core.hpp>

// Cumulative range geodesic filtering (Mould, NPAR 2012, equations 3 and 4).
//
// image:    nonempty, two-dimensional CV_8UC1 or CV_8UC3 image.
// maskSize: number of pixels to average, including the center (at least 1).
//           Values larger than the image are clamped to its pixel count.
// gamma:    finite, nonnegative weight of the local edge-crossing cost.
//
// Returns a new image of the same size and type. The input is never modified.
// Invalid arguments throw std::invalid_argument. Excessive image sizes or
// overflowing path costs throw std::overflow_error.
cv::Mat texturePreservingAbstraction(const cv::Mat& image,
                                    int maskSize = 160,
                                    double gamma = 1.0);

#endif
