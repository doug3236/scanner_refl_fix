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

// Get minimum of r, g, or b in an rgb array
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

// return top and bottom of image assuming white border
// if flip==true, return left and right by rotating image 90 degrees clockwise
pair<size_t, size_t> get_patch_ends(const Array2D<float>& g_in, bool flip)
{
    struct AveStd { float ave, std; };
    auto strips_info = [](const Array2D<float>& v) {
        //auto printx = [](const vector<AveStd>& v) {
        //    string fname="test\\patch_ends";
        //    static string fname_end = "0.txt";
        //    FILE* fp = fopen((fname+fname_end).c_str(), "wt");
        //    for (auto x : v)
        //        fprintf(fp, "%8.5f %8.5f\n", x.ave, x.std);
        //    fclose(fp);
        //    fname_end[0]++;
        //};
        float v_max=*std::max_element(v.v.begin(), v.v.end());
        float v_min=*std::min_element(v.v.begin(), v.v.end());
        auto get_boundary = [v_max, v_min](const vector<AveStd>& v)
        {
            for (int i = int(v.size()) / 3; i > 0; i--)
            {
                // allow for 15% reduction from max white value for white border detection
                if (v[i].ave > .85 * v_max && v[i].std < .05)
                {
                    for (int ii = 0; ii < 20; ii++)
                        if (v[i+ii].ave  < (v[i].ave+v[i+20].ave)/2.0f)
                            return i+ii;
                }
            }
            return 1;
        };
        vector<AveStd> strips(v.nr);
        for (int i = 0; i < v.nr; i++)
        {
            Statistics strip;
            for (int ii = 0; ii < v.nc; ii++)     // copy a row
                strip.clk(v(i, ii));
            strips[i].ave = strip.ave();
            strips[i].std = strip.std();
        }
        int top = get_boundary(strips);
        std::reverse(strips.begin(), strips.end());
        int bottom = int(strips.size()) - get_boundary(strips);
        //printx(strips);
        return std::pair<int,int>(top,bottom);
    };
    auto g = flip ? transpose(g_in) : g_in;
    //TiffWrite("test\\test_image.tif", g);
    return strips_info(g);
}

vector<std::pair<V3, V3>> get_dist_means(vector<BlockVal> const sorted_pixels)
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
PatchStats process_sample(const vector<BlockVal>& sample)
{
    PatchStats ret;
    ret.aves = get_dist_means(sample);

    vector<double> std_segment;
    vector<double> std_cum;
    RGB_Stat stats;
    RGB_Stat dist1, distcum;

    auto ps = sample.begin();
    auto pe = sample.end();
    for (; ps != sample.end(); ps++)    // 
    {
        dist1.clk(ps->pixel);
        distcum.clk(ps->pixel);
        if (ps == sample.end() - 1 || ps->dist != (ps + 1)->dist)
        {
            std_segment.push_back(dist1.norm());
            std_cum.push_back(distcum.norm());
            dist1.reset();
        }
    }
    reverse(std_cum.begin(), std_cum.end());
    reverse(std_segment.begin(), std_segment.end());
    ret.std_cum = std_cum;
    ret.std_segment = std_segment;
    static int passcnt;
    //printf("%3d  %7.4f  %7.4f  %7.4f\n", ++passcnt, std_segment[0], std_segment[4], std_segment[10]);

    size_t breakpoint = sample.size() / 2;    // include half of pixels closest to center
    while (sample[breakpoint].dist == sample[breakpoint + 1].dist)
        breakpoint++; // forward to next dist increment
    for (size_t i = 0; i < breakpoint; i++)
        stats.clk(sample[i].pixel);
    V3 ave = stats.ave();
    vector<PatchStats> set; set.reserve(breakpoint);
    for (size_t i = 0; i < breakpoint; i++)
        set.push_back(PatchStats{ sample[i].pixel, static_cast<float>(dist(sample[i].pixel, ave)) });

    // sort based on error from mean then remove the largest 1/3
    sort(set.begin(), set.end(), [](const PatchStats& a, const PatchStats& b) {return a.est_err < b.est_err; });
    set.resize(2 * set.size() / 3);

    RGB_Stat final;
    for (auto& x : set)
        final.clk(x.rgb);
    ret.rgb = final.ave();
    ret.est_err = final.std();
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


// calculate row and col counts then extract data into a 2D grid or patch info
vector<vector<PatchStats>> extract_patch_data(const Array2D<V3>& rgb)
{
    Array2D rgbmin = get_min_rgb(rgb);
    auto vs = get_patch_ends(rgbmin,false);     // top,bottom
    validate(vs.first > 0 && vs.second > 0, "No White Space at Top or Bottom detected.");
    rgbmin = rgbmin.clip(Extants{ int(vs.first), int(vs.second), 0, int(rgbmin.nc-1) });
    auto hs = get_patch_ends(rgbmin,true);      // left,right
    validate(hs.first > 0 && hs.second > 0, "No White Space at Left or Right detected.");
    auto [rows, cols] = get_rows_and_columns(rgb, hs, vs);
    float hdelta = static_cast<float>(hs.second - hs.first + 1) / cols;
    float vdelta = static_cast<float>(vs.second - vs.first + 1) / rows;
    vector<vector<vector<BlockVal>>> samples;
    for (size_t row = 0; row < rows; row++)
    {
        vector<vector<BlockVal>> row_samples;
        for (size_t col = 0; col < cols; col++)
        {
            vector<BlockVal> patch;
            size_t pos_00 = static_cast<size_t>(round(hs.first + hdelta * col));
            size_t pos_01 = static_cast<size_t>(round(hs.first + hdelta * (col + 1) - 1));
            size_t pos_10 = static_cast<size_t>(round(vs.first + vdelta * row));
            size_t pos_11 = static_cast<size_t>(round(vs.first + vdelta * (row + 1) - 1));
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
            // auto xxx=process_sample(patch);  // for debugging: just one process
            row_samples.push_back(patch);
            get_dist_means(patch);
        }
        samples.push_back(row_samples);
    }
    return process_samples(samples);
}

// Extract and add tif image data into charts
bool PatchCharts::init(string tiff_filename, bool landscape)
{
    auto scale = [](V3 x_in, float factor) {return V3{ x_in[0] * factor, x_in[1] * factor, x_in[2] * factor }; };
    PatchChart tmp;
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
                tmp.tiff_rgb =  rgb1;
            }
            else
                tmp.tiff_rgb = rgb;
        }
        //TiffWrite("x.tif", tmp.tiff_rgb,"");
        validate(tmp.tiff_rgb.nc > 0, "Invalid image patch file");
        if (tmp.tiff_rgb.dpi !=200)
            tmp.tiff_rgb = arrayRGBChangeDPI(tmp.tiff_rgb, 200);

        tmp.rgb = Array2D<V3>(tmp.tiff_rgb.nr, tmp.tiff_rgb.nc);
        for (int i = 0; i < tmp.tiff_rgb.nr; i++)
            for (int ii = 0; ii < tmp.tiff_rgb.nc; ii++)
            {
                tmp.rgb(i, ii)[0] = tmp.tiff_rgb(i, ii, 0);
                tmp.rgb(i, ii)[1] = tmp.tiff_rgb(i, ii, 1);
                tmp.rgb(i, ii)[2] = tmp.tiff_rgb(i, ii, 2);
            }
        tmp.patch_data = extract_patch_data(tmp.rgb);
        tmp.rows = static_cast<int>(tmp.patch_data.size());
        tmp.cols = static_cast<int>(tmp.patch_data[0].size());
        for (size_t i = 0; i < tmp.cols; i++)
            for (size_t ii = 0; ii < tmp.rows; ii++)
            {
                rgb255.push_back(scale(tmp.patch_data[ii][i].rgb,255.0f));
                rgb_std_255.push_back(scale(tmp.patch_data[ii][i].est_err,255.0f));
            }
        }
        catch (const char* e) {
            err_info = e;
    }
    charts.push_back(tmp);
    return true;
}

// Add LAB data to associated RGB data for use in generating ICC profiles
bool PatchCharts::add_lab_to_rgb(const vector<V3> & lab)
{
    if (rgb255.size() != lab.size())
    {
        err_info = "Patch count doesn't match measurement patch count\n";
        return false;
    }
    for (int i = 0; i < rgb255.size(); i++)
    {
        rgblab.push_back(V6{rgb255[i][0], rgb255[i][1], rgb255[i][2], lab[i][0], lab[i][1], lab[i][2] });
        for (auto& x:rgblab)
            if (x[4] > 85 && x[1] < 127)    // L* > 85 and G < 127, which should never occur
            {
                err_info = "Patches not matching high L* values\n";
                return false;
            }
    }
    return true;
}