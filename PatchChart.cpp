#define _CRT_SECURE_NO_WARNINGS
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

#include "PatchChart.h"
#pragma warning (disable : 4018 )

using std::vector;
using std::string;
using std::pair;
using std::array;

// Read RGB from tif file into a 2D Array
Array2D<V3> FromTiffReader(string arg)
{
    ArrayRGB rgb = TiffRead(arg.c_str(), 1.0);
    Array2D<V3> rgbout(rgb.nr, rgb.nc);
    for (int i = 0; i < rgb.nr; i++)
        for (int ii = 0; ii < rgb.nc; ii++)
        {
            rgbout(i, ii)[0] = rgb(i, ii, 0);
            rgbout(i, ii)[1] = rgb(i, ii, 1);
            rgbout(i, ii)[2] = rgb(i, ii, 2);
        }
    return rgbout;
}

/// <summary>
/// Get minimum of r, g, or b in an rgb array as gray scale array
/// </summary>
/// <param name="rgbin"></param>
/// <returns></returns>
Array2D<float> get_min_rgb(const Array2D<V3>& rgbin)
{
    Array2D<float> rgbout(rgbin.nr, rgbin.nc);
    for (int i = 0; i < rgbin.nr; i++)
        for (int ii = 0; ii < rgbin.nc; ii++)
        {
            rgbout(i, ii) = static_cast<float>(std::min({ rgbin(i,ii)[0], rgbin(i,ii)[1], rgbin(i,ii)[2] }));
        }

    return rgbout;
}

float get_tilt(const Array2D<float>& v, int offset)
{
    return 0;
}


struct AveStd { float ave, std; };

int get_boundary(const vector<AveStd>& v, float min, float max)
{
    for (int i = int(v.size()) / 3; i > 0; i--)
    {
        // allow for 15% reduction from max white value for white border detection
        if (v[i].ave > .85 * max && v[i].std < .05)
        {
            for (int ii = 0; ii < 20; ii++)
                if (v[i + ii].ave < (v[i].ave + v[i + 20].ave) / 2.0f)
                    return i + ii;
        }
    }
    return 1;
};

/// <summary>
/// Get top and bottom boundary of patches
/// </summary>
/// <param name="v"></param>
/// <returns></returns>
pair<int,int> strips_info(const Array2D<float>& v) {
    auto minmax = std::minmax_element(v.v.begin(), v.v.end());
    vector<AveStd> strips(v.nr);
    for (int i = 0; i < v.nr; i++)
    {
        Statistics strip;
        for (int ii = 0; ii < v.nc; ii++)     // copy a row
            strip.clk(v(i, ii));
        strips[i].ave = strip.ave();
        strips[i].std = strip.std();
    }
    int top = get_boundary(strips, *minmax.first, *minmax.second);
    std::reverse(strips.begin(), strips.end());
    int bottom = int(strips.size()) - get_boundary(strips, *minmax.first, *minmax.second);
    return std::pair<int, int>(top, bottom);
};


/// <summary>
/// return top and bottom of image assuming white border
/// </summary>
/// <param name="g_in"></param>
/// <param name="flip"></param> if true, rotate image 90 degrees clockwise
/// <returns></returns>
pair<size_t, size_t> get_patch_ends(const Array2D<float>& g_in, bool flip)
{
    auto g = flip ? transpose(g_in) : g_in;
    //TiffWrite("test\\test_image.tif", g);
    return strips_info(g);
}

/// <summary>
/// get ave and std for each set of pixels at fixed distances from patch edges
/// </summary>
/// <param name="sorted_pixels"></param>    sorted by descending distance from edge
/// <returns></returns>
vector<std::pair<V3, V3>> get_dist_means(vector<BlockVal> const &sorted_pixels)
{
    vector<std::pair<V3,V3>> ret(sorted_pixels[0].dist+1);
    int high = sorted_pixels[0].dist;
    int low = sorted_pixels[sorted_pixels.size()-1].dist;
    validate(high > 5, "too few pixels in patch, must be > 5 from center");
    validate(low ==0, "center must be '0' dist");
    auto start = sorted_pixels.begin();
    for (int i = high; i >= 0; i--)
    {
        RGB_Stat stat;
        while (start != sorted_pixels.end() && start->dist == i)
        {
            stat.clk(start->pixel);
            start++;
        }
        ret[i].first = stat.ave();
        ret[i].second = stat.std();
    }
    return ret;
}

// get statistics from a patch of pixels discarding outliers

/// <summary>
/// PatchStats::rgb has best average value of inner patch pixels
/// </summary>
/// <param name="sample"> vector containing pixel and distance from nearest edge
/// sorted starting from center pixels to edge pixels</param>
/// <returns></returns>
PatchStats process_sample(const vector<BlockVal>& sample)
{
    const float thresh_ctr = .3f;   // thresh_ctr of pixels that are closest to center
    const float thresh_std = .7f;   // thresh_std of lowest errs from the mean to discard lint/fiber reflections
    PatchStats ret;
    //ret.aves = get_dist_means(sample);

    RGB_Stat stats;
    RGB_Stat dist1;

    size_t breakpoint = int(round(thresh_ctr*sample.size()));
    while (sample[breakpoint].dist == sample[breakpoint + 1].dist)
        breakpoint++; // forward to next dist increment
    vector<pair<V3, float>> rgb_set(breakpoint);
    for (size_t i = 0; i < breakpoint; i++)
        stats.clk(sample[i].pixel);
    V3 ave = stats.ave();
    for (size_t i = 0; i < breakpoint; i++)
        rgb_set[i] = pair<V3, float>{ sample[i].pixel, dist(sample[i].pixel, ave)};

    // sort based on error from mean then remove the largest 1/3
    sort(rgb_set.begin(), rgb_set.end(), [](const pair<V3, float>& a, const pair<V3, float>& b) {return a.second < b.second; });
    rgb_set.resize(int(round(rgb_set.size()*thresh_std)));

    RGB_Stat final;
    for (auto& x : rgb_set)
        final.clk(x.first);
    ret.rgb = final.ave();
    ret.rgb_err = final.std();

    // look at variation in deciles from closest to center to edges
    int steps = int(sample.size() / ret.errs.size());
    for (int i = 0; i < ret.errs.size(); i++)
    {
        RGB_Stat stat;
        for (int ii = i * steps; ii < (i + 1) * steps; ii++)
            stat.clk(sample[ii].pixel);
        ret.errs[i] = dist(stat.std());
    }
    return ret;
}

// get statistics over the entire set of patches column by column
vector<vector<PatchStats>> process_samples(const vector<vector<vector<BlockVal>>>& samples)
{
    vector<vector<PatchStats>> ret;
    for (auto& x : samples) {
        vector<PatchStats> columns;
        for (auto& xx : x)
        {
            columns.push_back(process_sample(xx));
        }
        ret.push_back(columns);
    }
    return ret;
}

// return metrics for patches sizes from arg:start to maxRowsAndCols
// smallest metric indicates number of patches in row or column
vector<double> find_patch_metric(vector<double>& v, int start)
{
    vector<double> change(maxRowsAndCols+1);
    for (int cnt = start; cnt < change.size(); cnt++)
    {
        double delta = 1.0 * v.size() / cnt;
        double delta5 = round(.2 * delta);
        double largest = 0;
        auto skip = [&v, delta, delta5](int i, int id5) {
            auto x = static_cast<int>(std::round(i * delta + id5 * delta5));
            if (x >= v.size())
                x = static_cast<int>(v.size() - 1);
            return x;
        };
        for (int i = 0; i < cnt; i++)
        {
            auto t1 = std::accumulate(v.begin() + skip(i, 1), v.begin() + skip(i, 2), 0.);
            auto t2 = std::accumulate(v.begin() + skip(i, 2), v.begin() + skip(i, 3), 0.);
            auto t3 = std::accumulate(v.begin() + skip(i, 3), v.begin() + skip(i, 4), 0.);
            largest += std::max({ abs(t1 - t2),abs(t2 - t3) }) / delta5;
        }
        change[cnt] = largest;
    }
    return change;
}

// get row and col counts using minimums from find_patch_metric
VectorLocs get_rows_and_columns(const Array2D<V3>& rgb, VectorLocs hs, VectorLocs vs)
{
    VectorLocs ret;
    vector<double> cols(maxRowsAndCols+1);
    vector<double> rows(maxRowsAndCols+1);
    for (float pass = .005f; pass < .9975f; pass+=.005f) // scan 200 slices and accumulate patch location info
    {
        vector<double> green_slice_h;
        for (size_t i = hs.first; i < hs.second; i++)
            green_slice_h.push_back(rgb[int(vs.first + pass*(vs.second-vs.first))] [i] [1]);
        vector<double> cols_1 = find_patch_metric(green_slice_h, minRowsAndCols);
        for (size_t i = 0; i < cols_1.size(); i++)
            cols[i] += cols_1[i];

        vector<double> green_slice_v;
        for (size_t i = vs.first; i < vs.second; i++)
            green_slice_v.push_back(rgb[int(i)][int(hs.first + pass*(hs.second-hs.first))][1]);
        vector<double> rows_1 = find_patch_metric(green_slice_v, minRowsAndCols);
        for (int i = 0; i < rows_1.size(); i++)
            rows[i] += rows_1[i];
    }
    ret.first = std::min_element(rows.begin() + minRowsAndCols, rows.end()) - rows.begin();
    ret.second = std::min_element(cols.begin() + minRowsAndCols, cols.end()) - cols.begin();
    return ret;
}



Array2D<V3> refine_image(const Array2D<V3> &rgbin, const Array2D<float> rgbmin, const VectorLocs vs, const VectorLocs hs)
{
    auto rgb = rgbin;
    // if not enough white space to align assume proper registration
    if (vs.first < 10 || hs.first < 10 || rgbmin.nr-vs.second < 10 || rgbmin.nc - hs.second < 10)
        return rgb;
    auto spread = int(std::max(vs.second - vs.first, hs.second - hs.first)+1);
    vector<vector<int>> skew(19, vector<int>(spread));
    for (int angle_i = -9; angle_i <= 9; angle_i++)
        for (int i = 0; i < spread; i++)
            skew[angle_i+9][i] = int(std::round(angle_i * (float(spread/2-i) / (spread/2))));

    array<array<float, 19>, 19> s{};
    auto first_v = (spread / 2) - (hs.second - hs.first) / 2;
    auto first_h = (spread / 2) - (vs.second - vs.first) / 2;
    for (int angle_i = -9; angle_i <= 9; angle_i++)
        for (int offset = -9; offset <= 9; offset++)
        {
            Statistics variation;
            for (size_t i = hs.first; i < hs.second; i++)
            {
                variation.clk(rgbmin(int(offset + vs.first + skew[angle_i + 9][i - hs.first+first_v]), int(i - hs.first)));
            }
            s[angle_i + 9][offset + 9] = variation.std();
        }
    // min at s[15] for .5 degree tilt CCW
    array<float, 19> errsum;
    for (int i = 0; i < 19; i++)
        errsum[i] = std::accumulate(s[i].begin(), s[i].begin() + 9, 0.0f);
    auto angle_fit = std::min_element(errsum.begin(), errsum.end()) - errsum.begin();
    std::fill(rgb.v.begin(), rgb.v.end(), V3{ 1.0f,1.0f, 1.0f });

    for (int row=int(vs.first); row < int(vs.second); row++)
        for (int col = int(hs.first); col < int(hs.second); col++)
        {
            rgb(row, col) = rgbin(row+skew[angle_fit][col-hs.first+first_v],
                                  col+skew[18-angle_fit][row-vs.first+first_h]);
        }
    //TiffWrite("test\\testx.tif", rgb,"");
    return rgb;
}

// calculate row and col counts then extract data into a 2D grid of patch info
vector<vector<PatchStats>> extract_patch_data(const Array2D<V3>& rgbin)
{
    Array2D<float> rgbmin = get_min_rgb(rgbin);
    auto vs = get_patch_ends(rgbmin,false);     // locate top,bottom of patch grid
    validate(vs.first > 0 && vs.second > 0, "No White Space at Top or Bottom detected.");
    auto rgbmin1 = rgbmin.clip(Array2D<float>::Extants{ int(vs.first), int(vs.second), 0, int(rgbmin.nc-1) });
    auto hs = get_patch_ends(rgbmin1,true);      // locate left,right of patch grid
    validate(hs.first > 0 && hs.second > 0, "No White Space at Left or Right detected.");
    const auto rgb = refine_image(rgbin, rgbmin, vs, hs);   // align image so top is parallel
    auto [rows, cols] = get_rows_and_columns(rgb, hs, vs);
    float hdelta = static_cast<float>(hs.second - hs.first + 1) / cols;
    float vdelta = static_cast<float>(vs.second - vs.first + 1) / rows;
    vector<vector<vector<BlockVal>>> samples;
    for (size_t row = 0; row < rows; row++)
    {
        vector<vector<BlockVal>> row_samples;
        for (size_t col = 0; col < cols; col++)
        {
            size_t pos_00 = static_cast<size_t>(round(hs.first + hdelta * col));
            size_t pos_01 = static_cast<size_t>(round(hs.first + hdelta * (col + 1) - 1));
            size_t pos_10 = static_cast<size_t>(round(vs.first + vdelta * row));
            size_t pos_11 = static_cast<size_t>(round(vs.first + vdelta * (row + 1) - 1));
            vector<BlockVal> patch; patch.reserve((pos_01-pos_00 + 1)*(pos_11 - pos_10 + 1));
            //std::cout << pos_00 << " " << pos_01 << " " << pos_10 << " " << pos_11 << "\n";
            for (size_t i = pos_00; i < pos_01; i++)        // horizontal, second coord
                for (size_t ii = pos_10; ii < pos_11; ii++) // vertical, first coord
                {
                    BlockVal spot;
                    spot.pixel = rgb(static_cast<int>(ii), static_cast<int>(i));
                    spot.dist = static_cast<int>(std::min({ i - pos_00, ii - pos_10, pos_01 - i - 1, pos_11 - ii - 1 }));
                    patch.push_back(spot);
                }
            std::sort(patch.begin(), patch.end(), [](BlockVal& a, BlockVal& b) {return a.dist > b.dist; });
            row_samples.push_back(patch);
        }
        samples.push_back(row_samples);
    }
    return process_samples(samples);
}

// Extract and add tif image data into charts
bool PatchCharts::init(string tiff_filename, bool landscape)
{
    auto scale = [](V3 x_in, float factor) {return V3{ x_in[0] * factor, x_in[1] * factor, x_in[2] * factor }; };
    PatchChart chart;
    try
    {
        {
            ArrayRGB rgb = TiffRead(tiff_filename.c_str(), 1.0);
            if (landscape)      // transpose if landscape, Image top must be on left side
            {
                ArrayRGB rgb1 = rgb;
                std::swap(rgb1.nc, rgb1.nr);
                for (int color = 0; color < 3; color++)
                    for (int i = 0; i < rgb.nr; i++)
                        for (int ii = 0; ii < rgb.nc; ii++)
                            rgb1(ii, rgb.nr-i-1, color) = rgb(i, ii, color);  // Image top must be on left side!
                chart.tiff_rgb =  rgb1;
            }
            else
                chart.tiff_rgb = rgb;
        }
        //TiffWrite("x.tif", tmp.tiff_rgb,"");
        validate(chart.tiff_rgb.nc > 0, "Invalid image patch file");
        if (chart.tiff_rgb.dpi !=200)
            chart.tiff_rgb = arrayRGBChangeDPI(chart.tiff_rgb, 200);

        chart.rgb = Array2D<V3>(chart.tiff_rgb.nr, chart.tiff_rgb.nc);
        for (int i = 0; i < chart.tiff_rgb.nr; i++)
            for (int ii = 0; ii < chart.tiff_rgb.nc; ii++)
            {
                chart.rgb(i, ii)[0] = chart.tiff_rgb(i, ii, 0);
                chart.rgb(i, ii)[1] = chart.tiff_rgb(i, ii, 1);
                chart.rgb(i, ii)[2] = chart.tiff_rgb(i, ii, 2);
            }
        chart.patch_data = extract_patch_data(chart.rgb);
        chart.rows = static_cast<int>(chart.patch_data.size());
        chart.cols = static_cast<int>(chart.patch_data[0].size());
        for (size_t i = 0; i < chart.cols; i++)
            for (size_t ii = 0; ii < chart.rows; ii++)
            {
                // scale for 8 bit representation [0-255]
                rgb255.push_back(scale(chart.patch_data[ii][i].rgb,255.0f));
                rgb_std_255.push_back(scale(chart.patch_data[ii][i].rgb_err,255.0f));
            }
        }
        catch (const char* e) {
            err_info = e;
    }
    charts.push_back(chart);
    return true;
}

// Add LAB data to associated RGB data for use in generating ICC profiles
void PatchCharts::add_lab_to_rgb(const vector<V3> & lab)
{
    validate(rgb255.size() == lab.size(), "Patch count doesn't match measurement patch count");
    for (int i = 0; i < rgb255.size(); i++)
    {
        validate(!(lab[i][0] > 85 && rgb255[i][1] < 127), "Patches not matching high L* values");
        rgblab.push_back(V6{rgb255[i][0], rgb255[i][1], rgb255[i][2], lab[i][0], lab[i][1], lab[i][2] });
    }
}