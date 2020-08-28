/*
Copyright (c) <2020> <doug gray>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef PATCHCHART_H
#define PATCHCHART_H

#include <iostream>
#include "tiffresults.h"
#include "statistics.h"
#include "cgats.h"
#include "interpolate.h"
#include "validation.h"

using V3 = std::array<float, 3>;   // used to hold RGB, LAB or XYZ values
using V6 = std::array<float, 6>;   // used to hold RGBLAB value sets

/// <summary>
/// pixel and distance from patch edge
/// </summary>
struct BlockVal {
    V3 pixel;   // rgb pixel
    int dist;   // minimum distance of this pixel to patch boundary
};

/// <summary>
/// Average rgb, err of center patch
/// </summary>
struct PatchStats {
    V3 rgb{};           // average RGB of center, de-noised, patches
    V3 rgb_err{};       // std dev of center, de-noised, patches
    std::array<float,10> errs{};
    std::vector<std::pair<V3,V3>> aves; // used for debugging
};

constexpr int maxRowsAndCols=50;
constexpr int minRowsAndCols=8;

typedef std::pair<size_t, size_t> VectorLocs;

Array2D<V3> FromTiffReader(std::string arg);
Array2D<float> get_min_rgb(const Array2D<V3>& rgbin);
PatchStats process_sample(const std::vector<BlockVal>& sample);
std::vector<std::vector<PatchStats>> process_samples(const std::vector<std::vector<std::vector<BlockVal>>>& samples);
std::vector<double> find_patch_metric(std::vector<double>& v, int start);
VectorLocs get_rows_and_columns(const Array2D<V3>& rgb, VectorLocs hs, VectorLocs vs);
std::vector<std::vector<PatchStats>> extract_patch_data(const Array2D<V3>& rgb);

inline V3 operator+(const V3& x, const V3& y) { return V3{ x[0] + y[0], x[1] + y[1], x[2] + y[2] }; }
inline V3 operator-(const V3& x, const V3& y) { return V3{ x[0] - y[0], x[1] - y[1], x[2] - y[2] }; }
inline float dist(V3 x) { return std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]); }
inline float dist(V3 x, V3 y) { return dist(x - y); }

/// <summary>
/// Accumulates statistics over RGB pixels
/// </summary>
struct RGB_Stat {
    Statistics rgb[3];
    void clk(V3 pix) { rgb[0].clk(pix[0]); rgb[1].clk(pix[1]); rgb[2].clk(pix[2]); }
    V3 ave() { return V3{ rgb[0].ave(), rgb[1].ave(), rgb[2].ave() }; };
    V3 std() { return V3{ rgb[0].std(), rgb[1].std(), rgb[2].std() }; };
    void reset() { rgb[0].reset(); rgb[1].reset(); rgb[2].reset(); }
    float max_ave() { return std::max({ rgb[0].ave(), rgb[1].ave(), rgb[2].ave() }); }
    float max_std() { return std::max({ rgb[0].std(), rgb[1].std(), rgb[2].std() }); }
    float min_ave() { return std::min({ rgb[0].ave(), rgb[1].ave(), rgb[2].ave() }); }
    double norm() { return dist(V3{ rgb[0].std(), rgb[1].std(), rgb[2].std() }); }
    int n() { return rgb[0].n(); }
};


/// <summary>
/// Collection of tiff charts and associated data
/// </summary>
class PatchCharts   // collection of tif charts
{
public:
    class PatchChart {  // data for each tif image of patches
    public:
        ArrayRGB tiff_rgb{};
        Array2D<V3> rgb{ 0, 0 };
        std::vector<std::vector<PatchStats>> patch_data;
        std::vector<V3> rgb255{};
        int rows{};
        int cols{};
        std::string err_info{};
    };
    std::vector<PatchChart> charts{};
    std::string err_info{};          // overall error inf if not blank
    std::vector<V3> rgb255{};        // patch scan RGB values
    std::vector<V3> rgb_std_255{};   // patch scan RGB values
    std::vector<V6> rgblab{};        // optional: patch scan RGB with measurement Lab values
    std::vector<V6> rgblab_print{};  // optional: print RGB

    bool init(std::string tiff_filename, bool landscape);                // add a tif file
    void add_lab_to_rgb(const std::vector<V3> & lab);    // add in lab meas. data to rgb
};
#endif
