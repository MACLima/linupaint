#pragma once

#include "raster/image.h"

namespace lp {

Image flipHorizontal(const Image& img);
Image flipVertical(const Image& img);
// Clockwise rotation by 90, 180 or 270 degrees.
Image rotate(const Image& img, int degrees);
// Nearest-neighbour resize to an explicit size.
Image scaled(const Image& img, int w, int h);
// Paint's "Stretch": percentages per axis.
Image stretch(const Image& img, int percentX, int percentY);
// Paint's "Skew": degrees in (-90, 90); uncovered area is filled with `background`.
Image skew(const Image& img, int degreesX, int degreesY, Rgba background);
Image invertColors(const Image& img);
// Changes the canvas size keeping the top-left anchor; new area gets `background`.
Image resizeCanvas(const Image& img, int w, int h, Rgba background);
// Black and white (1 bit) conversion with a luminance threshold.
Image toMonochrome(const Image& img);
// Mask counterparts, used by free-form selections.
Mask flipHorizontal(const Mask& m);
Mask flipVertical(const Mask& m);
Mask rotate(const Mask& m, int degrees);
Mask scaled(const Mask& m, int w, int h);

} // namespace lp
