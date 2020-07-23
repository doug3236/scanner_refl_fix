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

#ifndef TIFFRESULTS_H
#define TIFFRESULTS_H

// Requires libtiff.  www.libtiff.org
// For some reason Adobe Photoshop Tiff files use a field type of
// 7 (UNDEFINED) for some extra metadata that needs to be ignored
// but may be printed. Patch the source or ignore the message.
// See line 1554 in tif_dirread.c

#include <tiffio.h>
#include <string>
#include <vector>
#include <array>
#include <cassert>
#include <cmath>
#include <numeric>
#include <future>
#include "interpolate.h"

// Common std types
using std::vector;
using std::string;
using std::array;
using std::ifstream;
using std::ios_base;
using std::ios;
using std::tuple;


// Utility Functions
class ArrayRGB;
void attach_profile(const std::string & profile, TIFF * out, const ArrayRGB & rgb);
void TiffWrite(const char *file, const ArrayRGB &rgb, const string &profile);
ArrayRGB TiffRead(const char *filename, float gamma);
tuple<ArrayRGB, int, int> getReflArea(const int dpi, InterpolateRefl& interpolate, const int use_this_size_if_not_0 = 0);
ArrayRGB generate_reflected_light_estimate(const ArrayRGB& image_reduced, const ArrayRGB& refl_area);
Array2D<float> generate_reflected_light_estimate(const Array2D<float>& image_reduced, const array<array<float,93>,93>& refl_area, float fill=0);
ArrayRGB arrayRGBChangeDPI(const ArrayRGB& imag_in, int new_dpi);

// uncomment to disable multi-threading of R,G, and B color channels
//#define DISABLE_ASYNC_THREADS
#ifdef DISABLE_ASYNC_THREADS
static auto launchType = std::launch::deferred;
#else
static auto launchType = std::launch::async;
#endif

// Floating point RGB array representing an image including some context info
// RGB values are stored in separate vectors since operations on each are independant
// and so can be easily multi-threaded. Values are normally in gamma=1 and are [0:1]
class ArrayRGB {
public:
    vector<float> v[3];			// separate vectors for each R, G, and B channels
    vector<uint8> profile;     // size is zero if no profile attached to image
    int dpi;
    int nc, nr;
    float gamma;
    bool from_16bits;
    ArrayRGB(int NR = 0, int NC = 0, int DPI=0, bool bits16=true, float gamma=1.7)
        : nc(NC), nr(NR), dpi(DPI), from_16bits(bits16),
          gamma(gamma) { for (auto& x:v) x.resize(NR*NC); }
    void resize(int nrows, int ncols) { nr = nrows; nc = ncols; for (auto& x:v) x.resize(nc*nr); }
    void fill(float red, float green, float blue);
    void copy(const ArrayRGB &from, int offsetx, int offsety);
    ArrayRGB subArray(int rs, int re, int cs, int ce);	// rs:row start, re: row end, etc.
    void copyColumn(int to, int from);
    void copyRow(int to, int from);
	float& operator()(int r, int c, int color) { return v[color][r*nc+c]; };
    float const & operator()(int r, int c, int color) const {return v[color][r*nc+c];}
    void scale(float factor);    // scale all array values by factor
};

// mod((6-1), 3) if not 0, subtract 3 for extra padding
// This function is used to downsize the original image in multiples of 2 and/or 3
// since high resolution is not needed for calculating extra light reflectance.
inline ArrayRGB downsample(const ArrayRGB &from, int rate)
{
	/*count rates
	c  3  2
	1  0  0
	2  2  1
	3  1  0
	4  0  1
	5  2  0
	6  1  1
	*/
	auto xtra = [](int rc, int rate) {  // calc needed extra row/col elements
		auto resid = (rc - 1) % rate;
		return resid == 0 ? 0 : rate - resid;
	};
	auto xtra_r = xtra(from.nr, rate); auto xtra_c = xtra(from.nc, rate);
	ArrayRGB fromEx(from.nr + 4 + xtra_r, from.nc + 4 + xtra_c);  // Expand sides by 2;
	fromEx.copy(from, 2, 2);

	for (int i = 0; i < xtra_c; i++)    // duplicate last column(s)
		fromEx.copyColumn(from.nc + 2 + i, from.nc + 1 + i);
	for (int i = 0; i < xtra_r; i++)    // duplicate last rows(s)
		fromEx.copyRow(from.nr + 2 + i, from.nr + 1 + i);
	for (int i = 1; i >= 0; i--)    // duplicate first column(s)
		fromEx.copyColumn(i, i + 1);
	for (int i = 1; i >= 0; i--)    // duplicate first rows(s)
		fromEx.copyRow(i, i + 1);
	fromEx.copyColumn(fromEx.nc - 2, fromEx.nc - 3);
	fromEx.copyColumn(fromEx.nc - 1, fromEx.nc - 3);
	fromEx.copyRow(fromEx.nr - 2, fromEx.nr - 3);
	fromEx.copyRow(fromEx.nr - 1, fromEx.nr - 3);


	int nr = (fromEx.nr - (rate == 2 ? 3 : 2)) / rate;
	int nc = (fromEx.nc - (rate == 2 ? 3 : 2)) / rate;

	ArrayRGB ret(nr, nc);
	// fspecial('gaussian',5,1.2)
	array<array<float, 5>, 5> smooth{
		0.007332f, 0.020779f, 0.029406f, 0.020779f, 0.007332f,
		0.020779f, 0.058888f, 0.083334f, 0.058888f, 0.020779f,
		0.029406f, 0.083334f, 0.117928f, 0.083334f, 0.029406f,
		0.020779f, 0.058888f, 0.083334f, 0.058888f, 0.020779f,
		0.007332f, 0.020779f, 0.029406f, 0.020779f, 0.007332f };

	for (int color = 0; color <= 2; color++)
	{
		for (int x = 0; x < nr; x++) {            // interate over destination array
			for (int y = 0; y < nc; y++)
			{
				int xs = rate * x;
				int ys = rate * y;
				float prodsum = 0;
				for (int i = 0; i < 5; i++) {
					for (int j = 0; j < 5; j++) {
						assert(xs + i < fromEx.nr);
						assert(ys + j < fromEx.nc);
						prodsum += smooth[i][j] * fromEx(xs + i, ys + j, color);
					}
				}
				ret(x, y, color) = prodsum;
			}
		}
	}
	ret.dpi = from.dpi / rate;
	return ret;
}

// This function is used to downsize a gray scale image by 2 or 3
// since high resolution is not needed for calculating extra light reflectance.
inline Array2D<float> downsample(const Array2D<float> &from, int rate)
{
	auto xtra = [](int rc, int rate) {  // calc needed extra row/col elements
		auto resid = (rc - 1) % rate;
		return resid == 0 ? 0 : rate - resid;
	};
	auto xtra_r = xtra(from.nr, rate); auto xtra_c = xtra(from.nc, rate);
	Array2D<float> fromEx(from.nr + 4 + xtra_r, from.nc + 4 + xtra_c);  // Expand sides by 2;
	fromEx.insert(from, 2, 2);

	int nr = (fromEx.nr - (rate == 2 ? 3 : 2)) / rate;
	int nc = (fromEx.nc - (rate == 2 ? 3 : 2)) / rate;

	// fspecial('gaussian',5,1.2)
	Array2D<float> ret(nr, nc);
	array<array<float, 5>, 5> smooth{
		0.007332f, 0.020779f, 0.029406f, 0.020779f, 0.007332f,
		0.020779f, 0.058888f, 0.083334f, 0.058888f, 0.020779f,
		0.029406f, 0.083334f, 0.117928f, 0.083334f, 0.029406f,
		0.020779f, 0.058888f, 0.083334f, 0.058888f, 0.020779f,
		0.007332f, 0.020779f, 0.029406f, 0.020779f, 0.007332f };

	for (int x = 0; x < nr; x++) {            // iterate over destination array
		for (int y = 0; y < nc; y++)
		{
			int xs = rate * x;
			int ys = rate * y;
			float prodsum = 0;
			for (int i = 0; i < 5; i++) {
				for (int j = 0; j < 5; j++) {
					assert(xs + i < fromEx.nr);
					assert(ys + j < fromEx.nc);
					prodsum += smooth[i][j] * fromEx(xs + i, ys + j);
				}
			}
			ret(x, y) = prodsum;
		}
	}
	return ret;
}


// f(0,0)(1-x)(1-y) +f(1,0)x(y-1)+f(0,1)(1-x)y + f(1,1)xy
// https://en.wikipedia.org/wiki/Bilinear_interpolation
inline float bilinear(ArrayRGB &correction, int r, int c, int reduction, int color)
{
    auto r0 = r/reduction;
    auto r1 = r/reduction+1;
    auto c0 = c/reduction;
    auto c1 = c/reduction+1;
    if (c1 > correction.nc-1) c1 = correction.nc-1;
    if (r1 > correction.nr-1) r1 = correction.nr-1;
    float dr = static_cast<float>(r%reduction)/reduction;
    float dc = static_cast<float>(c%reduction)/reduction;

    auto q00= correction(r0, c0, color);
    auto q01 = correction(r0, c1, color);
    auto q10 = correction(r1, c0, color);
    auto q11 = correction(r1, c1, color);
    auto v = q00*(1-dr)*(1-dc)+q10*dr*(1-dc)+q01*(1-dr)*dc+q11*dr*dc;
    return v;
}

#endif
