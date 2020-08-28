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

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <string>
#include <vector>
#include <algorithm>
#include "interpolate.h"
#include "tiffresults.h"
#include <tuple>

// debugging aid to print a vector<float> or vector<vector<float>>
template<class T>
void print(const std::vector<T>& v, const char* file)
{
    if constexpr (std::is_same <T, std::vector<float>()>)
    {
        FILE* fp = fopen(file, "wt");
        for (const auto& x : v)
        {
            for (auto y : x)
                fprintf(fp, "%8.5f ", y);
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    else
    {
        FILE* fp = fopen(file, "wt");
        for (auto x : v)
            fprintf(fp, "%8.5f\n", x);
        fclose(fp);
    }
}

// boundary locations where valid
struct PatchBounds {
    int top,bottom;     // vertical boundaries
    int left,right;     // horizontal boundaries
};

// Identifier to determine what portion of a square to average
// used to extract portions of squares that have differing scanner re-reflectance
// Warning: Do not change the order of members
struct Patch2 {
    int square;		// square index
    enum class Region { All, Center, Edges1, Edges2 } region;
    int weight;		// weight to count this measurement sample
};

using Array50dpi= std::array<std::array<float, 93>, 93>;
class ScanCalibration {
public:
    struct Extants {
        int top;
        int bottom;
        int left;
        int right;
    };
    struct ReflDistribution {float ave; std::vector<float> slice; std::vector<int> cnt;};
    struct Step10dpi { std::vector<float> horiz, vert; float base; };
    bool load(std::string cal_tif_file, const std::vector<float>& neutrals_for_gamma);  // load calibration tif file
    std::array<ReflDistribution,14> refl;           // stats and data for each of 14 squares
    Array2D<float> reflection10dpi;                 // reflection grid (19x19) 10 DPI, Normalized: DC gain==1
    std::array<Array2D<float>,14> squares_50dpi;    // arrays containing images of squares
    std::array<PatchBounds, 14> patch_bounds;       // boundaries of valid data in squares_50dpi
    void locate_boundaries(int i_patch);            // fill in Patch2 boundaries
    float calibration_quality(bool zero_gain=false);
    std::tuple<float, Array2D<float>*> get_patch_area(Patch2 patch, Array50dpi& reflx); // get patch subset average
    float get_patch13(float gain);                  // get simplified 3" patch center (for speed)
    float optimize_base_gain();
    Array50dpi make_refl_array(float gain);
    float base_gain;
private:
    void calculate_reflection_strips();
    float dark;
    std::array<std::array<float, 3>, 14> locs = // Position and size of 14 white/gray squares 
        { {{5.5f, 2, .15f}, { 5.5f, 2.75f, .15f},{ 5.5f, 3.5f, .15f},{ 5.5f, 4.25f, .15f},{ 5.5f, 5, .15f}, // Small gamma sqrs
        {6.5f, 2, .25f},{ 6.5f, 2.75f, .25f},{ 6.5f, 3.5f, .25f},{ 6.5f, 4.25f, .25f},{ 6.5f, 5, .25f},     // Large gamma sqrs
        {7.5f, 1.5f, .5f},{7.4f, 3.0f, .7f},{ 7.3f, 4.75f, .9f},{1.5f, 2, 3}}}; // Increasing white squares
    std::array<Extants,14> extants; // these are calculated from locs (above)
    ArrayRGB raw_image_in;      // raw image, full RGB unclipped boundaries
    Array2D<float> image;       // raw image clipped to black edges
    std::array<float,4> gamma;  // estimated gammas from high to lower luminance
    float DPI{ 200 };           // required pixels per inch
    int pix_round(float f) { return static_cast<int>(f + .5f) ;};       // round to int pixel
    int inch_2_pix(float f) { return static_cast<int>(DPI*f + .5f) ;};  // convert inches to discrete pixels
    float update_square_data(bool update_gamma, const std::vector<float>& neutrals_for_gamma); //find extants and reflectance on all squares
    float calculate_reflection_matrix();  // get .05" across 1" luminance increase values from horiz/vert
    Step10dpi get_steps_and_align(const std::vector<float>& horiz, const std::vector<float>& vert);
    std::vector<float> horiz;    // pixel spaced horizontal reflectance across 3"  square
    std::vector<float> vert;     // pixel spaced vertical reflectance across 3"  square
};

#endif
